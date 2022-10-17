#include "ipv6.h"
#include "ipv6_config.h"


#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#if defined(PARSE_TRACE)
#define IPV6_TRACE(...) printf(__VA_ARGS__)
#else
#define IPV6_TRACE(...)
#endif

#ifdef HAVE__SNPRINTF_S
#define platform_snprintf(buffer, bytes, format, ...) \
    _snprintf_s(buffer, bytes, _TRUNCATE, format, __VA_ARGS__)
#else
#define platform_snprintf(buffer, bytes, format, ...) \
    snprintf(buffer, bytes, format, __VA_ARGS__)
#endif


// Original core address RFC 3513: https://tools.ietf.org/html/rfc3513
// Replacement address RFC 4291: https://tools.ietf.org/html/rfc4291

const uint32_t IPV6_STRING_SIZE =
    sizeof "[1234:1234:1234:1234:1234:1234:1234:1234/128%longinterface]:65535";
#define IPV4_STRING_SIZE (sizeof "255.255.255.255:65535")

//
// Distinct states of parsing an address
//
typedef enum {
    STATE_NONE              = 0,
    STATE_ADDR_COMPONENT    = 1,
    STATE_V6_SEPARATOR      = 2,
    STATE_ZERORUN           = 3,
    STATE_CIDR              = 4,
    STATE_IFACE             = 5,
    STATE_PORT              = 6,
    STATE_POST_ADDR         = 7,
    STATE_ERROR             = 8,
} state_t;

//
// Characters are converted into event classes
// to trigger state transitions
//
typedef enum {
    EC_DIGIT                = 0,
    EC_HEX_DIGIT            = 1,
    EC_V4_COMPONENT_SEP     = 2,
    EC_V6_COMPONENT_SEP     = 3,
    EC_CIDR_MASK            = 4,
    EC_IFACE                = 5,
    EC_OPEN_BRACKET         = 6,
    EC_CLOSE_BRACKET        = 7,
    EC_WHITESPACE           = 8,
} eventclass_t;

//
// Flags to indicate persistent state in the reader
//
typedef enum {
    READER_FLAG_ZERORUN            = 0x00000001,   // indicates that the zerorun index is set
    READER_FLAG_ERROR              = 0x00000002,   // indicates an error occurred in parsing
    READER_FLAG_IPV4_EMBEDDING     = 0x00000004,   // indicates IPv4 embedding has occurred
    READER_FLAG_IPV4_COMPAT        = 0x00000008,   // indicates IPv4 compatible address
} ipv6_reader_state_flag_t;

//
// Reader state encapsulates the process of token-izing and processing
// the incoming address specification
//
typedef struct ipv6_reader_state_t {
    ipv6_address_full_t*        address_full;       // pointer to output address
    const char*                 error_message;      // null unless an error occurs, pointer must be static
    const char*                 input;              // pointer to input buffer
    state_t                     current;            // current state
    int32_t                     input_bytes;        // input buffer length in bytes
    int32_t                     position;           // current position in input buffer
    int32_t                     components;         // index of current component left to right
    int32_t                     token_position;     // position of token for current state
    int32_t                     token_len;          // length in characters of token
    int32_t                     separator;          // separator count
    int32_t                     brackets;           // bracket count, should go to 1 then 0 for [::1] address notation
    int32_t                     zerorun;            // component where run of zeros was begun ::1 would be 0, 1::2 would be 1
    int32_t                     v4_embedding;       // index where v4_embedding occurred
    int32_t                     v4_octets;          // number of octets provided for the v4 address
    uint32_t                    flags;              // flags recording state
    ipv6_diag_func_t            diag_func;          // callback for diagnostics
    void*                       user_data;          // user data passed to diag callback
} ipv6_reader_state_t;


//
// Enable this section to get a full dump of the parser
//

#if defined(PARSE_TRACE)
//--------------------------------------------------------------------------------
static const char* state_str (state_t state)
{
    switch (state) {
        case STATE_NONE:            return "state-none";
        case STATE_ADDR_COMPONENT:  return "state-addr-component";
        case STATE_V6_SEPARATOR:    return "state-v6-separator";
        case STATE_ZERORUN:         return "state-zero-run";
        case STATE_CIDR:            return "state-cidr";
        case STATE_IFACE:           return "state-iface";
        case STATE_PORT:            return "state-port";
        case STATE_POST_ADDR:       return "state-post-addr";
        case STATE_ERROR:           return "state-error";
        default: break;
    }
    return "<unknown>";
}

//--------------------------------------------------------------------------------
static const char* eventclass_str (eventclass_t input)
{
    switch (input) {
        case EC_DIGIT:              return "eventclass-digit";
        case EC_HEX_DIGIT:          return "eventclass-hex-digit";
        case EC_V4_COMPONENT_SEP:   return "eventclass-v4-component-sep";
        case EC_V6_COMPONENT_SEP:   return "eventclass-v6-component-sep";
        case EC_CIDR_MASK:          return "eventclass-cidr-mask";
        case EC_IFACE:              return "eventclass-iface";
        case EC_OPEN_BRACKET:       return "eventclass-open-bracket";
        case EC_CLOSE_BRACKET:      return "eventclass-close-bracket";
        case EC_WHITESPACE:         return "eventclass-whitespace";
        default:
            break;
    }

    return "<unknown>";
}
#endif // PARSE_TRACE

//
// Update the current state logging the transition
//
#define CHANGE_STATE(value) \
    IPV6_TRACE("  * %s -> %s %s:%u\n", \
        state_str(state->current), state_str(value), __FILE__, (uint32_t)__LINE__); \
    state->current = value;

#define BEGIN_TOKEN(offset) \
    IPV6_TRACE("  * %s: token begin at %u\n", state_str(state->current), state->position + offset); \
    state->token_position = state->position + offset; \
    state->token_len = 0; \

//
// Indicate the presence of an invalid event class character for the current state
//
#define INVALID_INPUT() \
    IPV6_TRACE("invalid input class (%d) in state: %s at position %d of '%s' (%c)\n", \
        input, state_str(state->current), state->position, state->input, state->input[state->position]); \
    ipv6_error(state, IPV6_DIAG_INVALID_INPUT, "Invalid input"); \
    return;

//
// Validate a condition in the parser state
//
//--------------------------------------------------------------------------------
// Fix the components, if less that 4 components they should be left aligned
// except for the last component.
// 3       -> 0.0.0.3
// 1.2     -> 1.0.0.2
// 1.2.3   -> 1.2.0.3
static void ipv4_fix_components (int32_t v4_octets, uint16_t components[IPV6_NUM_COMPONENTS])
{
    switch (v4_octets) {
        case 1:
            components[1] = components[0] >> 8;
            components[0] = 0;
            break;
        case 2:
            components[1] = components[0] & 0xff;
            components[0] &= 0xff00;
            break;
        case 3:
            components[1] >>= 8;
            break;
        default:
            break;
    }
}

#define VALIDATE(msg, diag, cond, action) \
    if (!(cond)) { \
        IPV6_TRACE("  failed '!" #cond "' in state: %s at position %d of '%s'\n\n", \
            state_str(state->current), state->position, state->input); \
        ipv6_error(state, diag, msg); \
        action; \
    }

//--------------------------------------------------------------------------------
// Indicate error, function here for breakpoints
static void ipv6_error (ipv6_reader_state_t* state,
    ipv6_diag_event_t event,
    const char* message)
{
    ipv6_diag_info_t info;
    info.message = message;
    info.input = state->input;
    info.position = state->position;

    state->diag_func(event, &info, state->user_data);
    state->flags |= READER_FLAG_ERROR;
    state->error_message = message;
    CHANGE_STATE(STATE_ERROR);
}

//--------------------------------------------------------------------------------
static int32_t read_decimal_token (ipv6_reader_state_t* state)
{
    VALIDATE("Invalid token",
            IPV6_DIAG_INVALID_DECIMAL_TOKEN,
            state->token_position + state->token_len <= state->input_bytes,
            return 0);

    const char* cp = state->input + state->token_position;
    const char* ep = cp + state->token_len;
    int32_t accumulate = 0;
    int32_t digit;
    while (*cp && cp < ep) {
        switch (*cp) {
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                digit = *cp - '0';
                accumulate = (accumulate * 10) + digit;
                break;

            default:
                ipv6_error(state, IPV6_DIAG_INVALID_INPUT, "Non-decimal in token input");
                return 0;
        }
        cp++;
    }

    return accumulate;
}

//--------------------------------------------------------------------------------
static int32_t read_hexidecimal_token (ipv6_reader_state_t* state)
{
    VALIDATE("Invalid token",
            IPV6_DIAG_INVALID_HEX_TOKEN,
            state->token_position + state->token_len <= state->input_bytes,
            return 0);

    const char* cp = state->input + state->token_position;
    const char* ep = cp + state->token_len;
    int32_t accumulate = 0;
    int32_t digit;
    while (*cp && cp < ep) {
        switch (*cp) {
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                digit = (*cp - '0');
                break;

            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                digit = 10 + (*cp - 'a');
                break;

            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                digit = 10 + (*cp - 'A');
                break;

            default:
                ipv6_error(state, IPV6_DIAG_INVALID_INPUT, "Non-hexidecimal token input");
                return 0;
        }
        accumulate = (accumulate << 4) | digit;
        cp++;
    }

    return accumulate;
}

//--------------------------------------------------------------------------------
// Move an address component from the state to the output
static void ipv6_parse_component (ipv6_reader_state_t* state) {
    int32_t component = read_hexidecimal_token(state);

    IPV6_TRACE("  * ipv6 address component %4x (%d)\n", (uint16_t)component, component);

    VALIDATE("Only 8 16bit components are allowed",
            IPV6_DIAG_V6_BAD_COMPONENT_COUNT,
            state->components < 8,
            return);

    VALIDATE("IPv6 address components must be <= 65535",
            IPV6_DIAG_V6_COMPONENT_OUT_OF_RANGE,
            component <= 0xffff,
            return);

    state->address_full->address.components[state->components] = (uint16_t)component;
    state->components++;

    state->token_position = 0;
    state->token_len = 0;
}

//--------------------------------------------------------------------------------
static void ipv4_parse_component (ipv6_reader_state_t* state) {
    int32_t octet = read_decimal_token(state);

    IPV6_TRACE("  * ipv4 address octet %2x (%d)\n", (uint8_t)octet, octet);

    VALIDATE("Only 4 8bit components are allowed in an IPv4 embedding",
        IPV6_DIAG_V4_BAD_COMPONENT_COUNT,
        state->v4_octets < 4,
        return);

    VALIDATE("IPv4 address components must be <= 255",
        IPV6_DIAG_V4_COMPONENT_OUT_OF_RANGE,
        octet <= 0xff,
        return);

    VALIDATE("IPv4 embedding must have at least 32bits",
        IPV6_DIAG_IPV4_REQUIRED_BITS,
        state->v4_embedding <= 6,
        return);

    // embed the octets such that they can be trivially treated as host order
    // node values e.g.: INADDR_LOOPBACK == components[0] << 16 | components[1]
    // octet 0,1 -> component embedding+0
    // octet 2,3 -> component embedding+1
    // even octets are in shifted to the upper 8 bits of the component
    uint16_t* addr_component = &state->address_full->address.components[state->v4_embedding + (state->v4_octets / 2)];
    const uint32_t shift = (1 - (state->v4_octets & 1)) * 8;
    *addr_component |= (uint16_t)octet << shift;

    state->v4_octets++;
    state->token_position = 0;
    state->token_len = 0;
}

//--------------------------------------------------------------------------------
static void ipvx_parse_component (ipv6_reader_state_t* state) {
    if (state->flags & READER_FLAG_IPV4_EMBEDDING) {
        ipv4_parse_component(state);
    } else {
        ipv6_parse_component(state);
    }
}

//--------------------------------------------------------------------------------
static void ipvx_parse_cidr (ipv6_reader_state_t* state) {
    int32_t mask = read_decimal_token(state);

    VALIDATE("CIDR mask must be between 0 and 128 bits",
        IPV6_DIAG_INVALID_CIDR_MASK,
        mask > -1 && mask < 129,
        return);

    state->address_full->mask = (uint32_t)mask;
    state->address_full->flags |= IPV6_FLAG_HAS_MASK;
}

//--------------------------------------------------------------------------------
static void ipvx_parse_port (ipv6_reader_state_t* state) {
    int32_t port = read_decimal_token(state);

    VALIDATE("Port must be between 0 and 65535",
        IPV6_DIAG_INVALID_PORT,
        port > -1 && port <= 0xffff,
        return);

    state->address_full->port = (uint16_t)port;
    state->address_full->flags |= IPV6_FLAG_HAS_PORT;
}

//--------------------------------------------------------------------------------
//
// State transition function for parser, given a current state and a event class input
// the state will be updated for a next state or accumulate data within the current state
//
static void ipv6_state_transition (
    ipv6_reader_state_t* state,
    eventclass_t input)
{
    IPV6_TRACE("  * transition input: %s <- %s\n", state_str(state->current), eventclass_str(input));

    switch (state->current) {
        default:
        case STATE_ERROR:
            break;

        case STATE_NONE:
            switch (input) {
                case EC_DIGIT:
                case EC_HEX_DIGIT:
                    CHANGE_STATE(STATE_ADDR_COMPONENT);
                    BEGIN_TOKEN(0);
                    state->token_len++;
                    break;

                case EC_OPEN_BRACKET:
                    VALIDATE("Only one set of balanced brackets are allowed",
                        IPV6_DIAG_INVALID_BRACKETS,
                        state->brackets == 1,
                        return);
                    break;

                case EC_CLOSE_BRACKET:
                    CHANGE_STATE(STATE_POST_ADDR);
                    break;

                case EC_V6_COMPONENT_SEP:
                    CHANGE_STATE(STATE_V6_SEPARATOR);
                    break;

                case EC_CIDR_MASK:
                    CHANGE_STATE(STATE_CIDR);
                    BEGIN_TOKEN(1);
                    break;

                case EC_WHITESPACE:
                    break;

                default:
                    INVALID_INPUT();
                    break;
            }
            break;

        case STATE_ADDR_COMPONENT:
            switch (input) {
                case EC_DIGIT:
                case EC_HEX_DIGIT:
                    state->token_len++;
                    break;

                case EC_CLOSE_BRACKET:
                    ipvx_parse_component(state);
                    CHANGE_STATE(STATE_POST_ADDR);
                    break;

                case EC_WHITESPACE:
                    ipvx_parse_component(state);
                    CHANGE_STATE(STATE_NONE);
                    break;

                case EC_V6_COMPONENT_SEP:
                    // Allow IPv4 compatible addresses to contain dotted-quad:port
                    if (state->flags & READER_FLAG_IPV4_COMPAT) {
                        ipvx_parse_component(state);
                        CHANGE_STATE(STATE_PORT);
                        BEGIN_TOKEN(1);
                        break;
                    }

                    // Else treat this is as an address component separator
                    VALIDATE("IPv4 embedding only allowed in last 32 address bits",
                        IPV6_DIAG_IPV4_INCORRECT_POSITION,
                        (state->flags & READER_FLAG_IPV4_EMBEDDING) == 0,
                        return);
                    ipvx_parse_component(state);
                    CHANGE_STATE(STATE_V6_SEPARATOR);
                    break;

                case EC_V4_COMPONENT_SEP:
                    // Mark the embedding point, don't allow IPv6 address components after this point
                    if (!(state->flags & READER_FLAG_IPV4_EMBEDDING)) {
                        state->v4_embedding = state->components;
                        state->flags |= READER_FLAG_IPV4_EMBEDDING;

                        VALIDATE("IPv4 embedding requires 32 bits of address space",
                            IPV6_DIAG_IPV4_REQUIRED_BITS,
                            state->components < 7,
                            return);

                        // Backwards compatibility marker for pure IPv4 address
                        if (!(state->flags & READER_FLAG_ZERORUN) && state->components == 0) {
                            state->flags |= READER_FLAG_IPV4_COMPAT;
                        }

                        // Reserve the components
                        state->components += 2;

                    }
                    ipvx_parse_component(state);

                    // There is no separate state for IPv4 component separators
                    CHANGE_STATE(STATE_NONE);
                    break;

                case EC_IFACE:
                    ipvx_parse_component(state);
                    CHANGE_STATE(STATE_IFACE);
                    break;

                case EC_CIDR_MASK:
                    ipvx_parse_component(state);
                    CHANGE_STATE(STATE_CIDR);
                    BEGIN_TOKEN(1);
                    break;

                default:
                    INVALID_INPUT();
                    break;

            }
            break;

        case STATE_V6_SEPARATOR:
            switch (input) {
                case EC_V6_COMPONENT_SEP:
                    // Second component separator
                    VALIDATE("Only one abbreviation of zeros is allowed",
                        IPV6_DIAG_INVALID_ABBREV,
                        (state->flags & READER_FLAG_ZERORUN) == 0,
                        return)

                    // Mark the position of the run
                    state->zerorun = state->components;
                    state->flags |= READER_FLAG_ZERORUN;

                    IPV6_TRACE("  * zero run index: %d\n", state->zerorun);
                    break;

                case EC_WHITESPACE:
                    CHANGE_STATE(STATE_NONE);
                    break;

                case EC_CLOSE_BRACKET:
                    CHANGE_STATE(STATE_POST_ADDR);
                    break;

                case EC_OPEN_BRACKET:
                    VALIDATE("Invalid open bracket after address separator",
                        IPV6_DIAG_INVALID_BRACKETS,
                        false,
                        return)
                    break;

                case EC_DIGIT:
                case EC_HEX_DIGIT:
                    CHANGE_STATE(STATE_ADDR_COMPONENT);
                    BEGIN_TOKEN(0);
                    state->token_len++;
                    break;

                case EC_IFACE:
                    CHANGE_STATE(STATE_IFACE);
                    break;

                case EC_CIDR_MASK:
                    BEGIN_TOKEN(0);
                    CHANGE_STATE(STATE_CIDR);
                    BEGIN_TOKEN(1);
                    break;

                default:
                    INVALID_INPUT();
                    break;
            }
            break;

        case STATE_IFACE:
            // TODO: identify all valid interface characters
            switch (input) {
                case EC_WHITESPACE:
                    CHANGE_STATE(STATE_NONE);
                    break;

                case EC_CLOSE_BRACKET:
                    CHANGE_STATE(STATE_POST_ADDR);
                    break;

                default:
                    break;
            }
            break;

        case STATE_CIDR:
            switch (input) {
                case EC_DIGIT:
                    state->token_len++;
                    break;

                case EC_CLOSE_BRACKET:
                    ipvx_parse_cidr(state);
                    CHANGE_STATE(STATE_POST_ADDR);
                    break;

                case EC_WHITESPACE:
                    ipvx_parse_cidr(state);
                    CHANGE_STATE(STATE_NONE);
                    break;

                case EC_IFACE:
                    ipvx_parse_cidr(state);
                    CHANGE_STATE(STATE_IFACE);
                    break;

                default:
                    INVALID_INPUT();
                    break;
            }
            break;

        case STATE_POST_ADDR:
            switch (input) {
                case EC_WHITESPACE:
                    break;

                case EC_V6_COMPONENT_SEP:
                    CHANGE_STATE(STATE_PORT);
                    BEGIN_TOKEN(1); // start the port token after the separator
                    break;

                default:
                    INVALID_INPUT();
                    break;

            }
            break;

        case STATE_PORT:
            switch (input) {
                case EC_DIGIT:
                    state->token_len++;
                    break;

                case EC_WHITESPACE:
                    ipvx_parse_port(state);
                    CHANGE_STATE(STATE_NONE);
                    break;

                default:
                    INVALID_INPUT();
            }
            break;

    } // end state switch
}

//--------------------------------------------------------------------------------
static bool ipv4_parse_single(ipv6_reader_state_t *state) {
    // Special case single numbers as shortcut IPv4
    state->v4_octets = 0;
    state->components = 0;
    state->address_full->flags |= IPV6_FLAG_IPV4_COMPAT;
    state->address_full->address.components[0] = state->address_full->address.components[1] = 0;
    state->token_position = 0;
    state->token_len = state->input_bytes;

    ipv4_parse_component(state);

    if ((state->flags & READER_FLAG_ERROR) != 0) {
        return false;
    }

    ipv4_fix_components(state->v4_octets, state->address_full->address.components);

    return true;
}

//--------------------------------------------------------------------------------
static bool ipv4_parse_single_port(ipv6_reader_state_t *state) {
    // Special case single number with a port as a shortcut IPv4
    //   treat the second component as the port
    state->components = 0;
    state->v4_octets = 0;
    state->address_full->address.components[1] = 0;
    state->address_full->flags |= IPV6_FLAG_IPV4_COMPAT | IPV6_FLAG_HAS_PORT;
    char *port = strchr(state->input, ':');
    if (!port) {
        return false;
    }

    // re-parse the first octet as an IPV4 component
    state->token_position = 0;
    state->token_len = (int32_t)(port - state->input);
    ipv4_parse_component(state);
    if ((state->flags & READER_FLAG_ERROR) != 0) {
        return false;
    }

    state->token_position = (int32_t)(port + 1 - state->input);
    state->token_len = state->input_bytes - state->token_position;
    ipvx_parse_port(state);
    if ((state->flags & READER_FLAG_ERROR) != 0) {
        return false;
    }

    ipv4_fix_components(state->v4_octets, state->address_full->address.components);
    return true;
}

//--------------------------------------------------------------------------------
bool IPV6_API_DEF(ipv6_from_str_diag) (
    const char* input,
    size_t input_bytes,
    ipv6_address_full_t* out,
    ipv6_diag_func_t func,
    void* user_data)
{
    const char *cp = input;
    const char* ep = input + input_bytes;
    ipv6_reader_state_t state;

    memset(&state, 0, sizeof(state));

    state.diag_func = func;
    state.user_data = user_data;

    if (!input || !*input || !out) {
        ipv6_error(&state, IPV6_DIAG_INVALID_INPUT,
            "Invalid input");
        return false;
    }

    if (input_bytes > IPV6_STRING_SIZE) {
        ipv6_error(&state, IPV6_DIAG_STRING_SIZE_EXCEEDED,
            "Input string size exceeded");
        return false;
    }

    memset(out, 0, sizeof(ipv6_address_full_t));

    state.current = STATE_NONE;
    state.input = input;
    state.input_bytes = (int32_t)input_bytes;
    state.address_full = out;

    while (*cp && cp < ep) {
        IPV6_TRACE(
            "  * parse state: %s, cp: '%c' (%02x) position: %d, flags: %08x\n",
            state_str(state.current),
            *cp,
            *cp,
            state.position,
            state.flags);

        switch (*cp) {
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                ipv6_state_transition(&state, EC_DIGIT);
                break;

            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                ipv6_state_transition(&state, EC_HEX_DIGIT);
                break;

            case ':':
                ipv6_state_transition(&state, EC_V6_COMPONENT_SEP);
                break;

            case '.':
                ipv6_state_transition(&state, EC_V4_COMPONENT_SEP);
                break;

            case '/':
                ipv6_state_transition(&state, EC_CIDR_MASK);
                break;

            case '%':
                ipv6_state_transition(&state, EC_IFACE);
                break;

            case '[':
                state.brackets++;
                ipv6_state_transition(&state, EC_OPEN_BRACKET);
                break;

            case ']':
                ipv6_state_transition(&state, EC_CLOSE_BRACKET);
                break;

            case ' ':
            case '\t':
            case '\n':
            case '\r':
                ipv6_state_transition(&state, EC_WHITESPACE);
                break;


            default:
                ipv6_error(&state, IPV6_DIAG_INVALID_INPUT_CHAR,
                    "Invalid input character");
                break;
        }

        // Exit the parse if the last state change triggered an error
        if (state.flags & READER_FLAG_ERROR) {
            return false;
        }

        cp++;
        state.position++;
    }

    // Treat the end of input as whitespace to simplify state transitions
    ipv6_state_transition(&state, EC_WHITESPACE);

    // Early out if there was an error processing the string
    if ((state.flags & READER_FLAG_ERROR) != 0) {
        return false;
    }

    // If an IPv4 compatible address was specified the rest of the IPv6 collapsing
    // rules can be skipped
    if ((state.flags & READER_FLAG_IPV4_COMPAT) != 0) {
        state.address_full->flags |= IPV6_FLAG_IPV4_COMPAT;
        ipv4_fix_components(state.v4_octets, state.address_full->address.components);
        return true;
    }

    // Mark the presence of embedded IPv4 addresses
    if (state.flags & READER_FLAG_IPV4_EMBEDDING) {
        if (state.v4_octets != 4) {
            ipv6_error(&state, IPV6_DIAG_V4_BAD_COMPONENT_COUNT,
                    "IPv4 address embedding was used but required 4 octets");
            return false;
        } else {
            state.address_full->flags |= IPV6_FLAG_IPV4_EMBED;
        }
    }

    // If there was no abbreviated run all components should be specified
    if ((state.flags & READER_FLAG_ZERORUN) == 0) {

        if (state.components == 1) {
            return ipv4_parse_single(&state);
        }
        else if (state.components == 2) {
            return ipv4_parse_single_port(&state);
        }
        if (state.components < IPV6_NUM_COMPONENTS) {
            ipv6_error(&state, IPV6_DIAG_V6_BAD_COMPONENT_COUNT,
                "Invalid component count");
            return false;
        }
        return true;
    }

    uint16_t dst[IPV6_NUM_COMPONENTS] = {0, };
    uint16_t* src = out->address.components;

    // Number of components moving
    int32_t move_count = state.components - state.zerorun;
    int32_t target = IPV6_NUM_COMPONENTS - move_count;
    if (move_count < 0 || move_count > IPV6_NUM_COMPONENTS) {
        IPV6_TRACE("invalid move_count: %d\n", move_count);
        return false;
    }
    if (target < 0 || target + move_count > IPV6_NUM_COMPONENTS) {
        IPV6_TRACE("invalid target location: %d:%d\n", target, move_count);
        return false;
    }

    // Copy the right side of the zero run
    memcpy(&dst[target], &src[state.zerorun], move_count * sizeof(uint16_t));

    // Copy the left side of the zero run
    memcpy(&dst[0], &src[0], state.zerorun * sizeof(uint16_t));

    // Everything else is zero, so just copy the destination array into the output directly
    memcpy(&(out->address.components[0]), &dst[0], IPV6_NUM_COMPONENTS * sizeof(uint16_t));

    return true;
}

//--------------------------------------------------------------------------------
static void ipv6_default_diag (
    ipv6_diag_event_t event,
    const ipv6_diag_info_t* info,
    void* user_data)
{
    (void)event;
    (void)info;
    (void)user_data;
}

//--------------------------------------------------------------------------------
bool IPV6_API_DEF(ipv6_from_str) (
    const char* input,
    size_t input_bytes,
    ipv6_address_full_t* out)
{
    return ipv6_from_str_diag(input, input_bytes, out, ipv6_default_diag, NULL);
}

#define OUTPUT_TRUNCATED() \
    IPV6_TRACE("  ! buffer truncated at position %u\n", (uint32_t)(wp - output)); \
    output_bytes = 0; \
    *output = '\0';

//--------------------------------------------------------------------------------
size_t IPV6_API_DEF(ipv6_to_str) (
    const ipv6_address_full_t* in,
    char *output,
    size_t output_bytes)
{
    if (!in || !output) {
        return 0;
    }

    if (output_bytes < 4) {
        return 0;
    }

    *output = '\0';

    const uint16_t* components = in->address.components;
    char* wp = output; // write pointer
    const char* ep = output + output_bytes - 1; // end pointer with one octet for nul
    char token[IPV4_STRING_SIZE];
    token[0] = '\0';

    // If the address is an IPv4 compatible address shortcut the IPv6 rules and 
    // print an address or address:port
    if (in->flags & IPV6_FLAG_IPV4_COMPAT) {
        int32_t n = platform_snprintf(token, sizeof(token), "%d.%d.%d.%d",
                          components[0] >> 8,
                          components[0] & 0xff,
                          components[1] >> 8,
                          components[1] & 0xff);
        // Add the port
        if (in->flags & IPV6_FLAG_HAS_PORT) {
            platform_snprintf(token + n, sizeof(token) - n, ":%d", in->port);
        }
        const char *cp = token;
        while (wp < ep && *cp) {
            *wp++ = *cp++;
        }

        // Terminate string
        output_bytes = (size_t)(ptrdiff_t)(wp - output);
        *wp++ = '\0';
        return output_bytes;
    }

    // For each component find the length of 0 digits that it covers (including
    // itself), if that span is the current longest span of 0 digits record the
    // position
    uint32_t spans_position = 0;
    uint32_t longest_span = 0;
    uint32_t longest_position = 0;
    uint8_t spans[IPV6_NUM_COMPONENTS] = { 0, };
    for (uint32_t i = 0; i < IPV6_NUM_COMPONENTS; ++i) {
        if (components[i]) {
            if (spans[spans_position] > longest_span) {
                longest_position = spans_position;
                longest_span = spans[spans_position];
            }
            spans_position = i + 1;
        }
        else {
            spans[spans_position]++;
        }
    }

    // Check the last identified span
    if (spans_position < IPV6_NUM_COMPONENTS && spans[spans_position] > longest_span) {
        longest_position = spans_position;
        longest_span = spans[spans_position];
    }

    // Bracket the address to supply a port
    if (in->flags & IPV6_FLAG_HAS_PORT) {
        *wp++ = '[';
    }

    // Emit all of the components
    for (uint32_t i = 0; i < IPV6_NUM_COMPONENTS; ++i) {
        const char* cp = token;

        // Write out the last two components as the IPv4 embed
        if (i == IPV4_EMBED_INDEX && in->flags & IPV6_FLAG_IPV4_EMBED) {
            const uint32_t host_ipv4 = components[IPV4_EMBED_INDEX] << 16 | components[IPV4_EMBED_INDEX + 1];
            platform_snprintf(
                token,
                sizeof(token),
                "%d.%d.%d.%d",
                (uint8_t)(host_ipv4 >> 24),
                (uint8_t)(host_ipv4 >> 16),
                (uint8_t)(host_ipv4 >> 8),
                (uint8_t)(host_ipv4));
            i++;
        } else {
            platform_snprintf(
                token,
                sizeof(token),
                "%x",
                components[i]);
        }

        // Skip the longest span of zeros by emitting the double colon abbreviation instead
        // of the token and continuing on the component at the end of the span
        if (i == longest_position && longest_span > 1) {
            if (wp + 2 >= ep) {
                OUTPUT_TRUNCATED();
                return output_bytes;
            }

            // The previous component already emitted a separator, or this is the
            // the first separator
            if (i > 0) {
                *wp++ = ':';
            } else {
                *wp++ = ':';
                *wp++ = ':';
            }
            i += (longest_span - 1);
            continue;
        } else {
            // Copy the token up to the terminator
            while (wp < ep && *cp) {
                *wp++ = *cp++;
            }

            if (i < IPV6_NUM_COMPONENTS - 1 && wp < ep) {
                *wp++ = ':';
            }
        }

        if (wp == ep) {
            // Truncated, return a deterministic result
            OUTPUT_TRUNCATED();
            return output_bytes;
        }
    }

    if (in->flags & IPV6_FLAG_HAS_MASK) {
        platform_snprintf(token, sizeof(token), "/%u", in->mask);
        const char* cp = token;
        while (wp < ep && *cp) {
            *wp++ = *cp++;
        }
    }

    if (in->flags & IPV6_FLAG_HAS_PORT) {
        platform_snprintf(token, sizeof(token), "]:%hu", in->port);
        const char* cp = token;
        while (wp < ep && *cp) {
            *wp++ = *cp++;
        }
    }

    // Output truncated
    if (wp == ep) {
        OUTPUT_TRUNCATED();
    }
    else {
        output_bytes = (size_t)(ptrdiff_t)(wp - output);
        *wp = '\0';
    }
    return output_bytes;
}

//--------------------------------------------------------------------------------
ipv6_compare_result_t IPV6_API_DEF(ipv6_compare) (
    const ipv6_address_full_t* a,
    const ipv6_address_full_t* b,
    uint32_t ignore_flags)
{
    const uint16_t* a_components;
    const uint16_t* b_components;
    uint32_t num_components;

    // Mask out flags for comparison
    uint32_t compare_flags = (IPV6_FLAG_HAS_MASK | IPV6_FLAG_HAS_PORT) & ~ignore_flags;

    // Treat any compatibility or embedded address as IPv4
#define HAS_IPV4(flags) (((flags) & (IPV6_FLAG_IPV4_COMPAT|IPV6_FLAG_IPV4_EMBED)) != 0)

    // Special case format selection:
    //
    // Allow comparison between embedded and compatible addresses
    //
    if (0 != (ignore_flags & (IPV6_FLAG_IPV4_EMBED|IPV6_FLAG_IPV4_COMPAT))
        && (HAS_IPV4(a->flags) || HAS_IPV4(b->flags)))
    {
        num_components = IPV4_NUM_COMPONENTS;
        if (a->flags & IPV6_FLAG_IPV4_COMPAT)
            a_components = &a->address.components[0];
        else
            a_components = &a->address.components[IPV4_EMBED_INDEX];
        if (b->flags & IPV6_FLAG_IPV4_COMPAT)
            b_components = &b->address.components[0];
        else
            b_components = &b->address.components[IPV4_EMBED_INDEX];
    }
    else {
        compare_flags |= (IPV6_FLAG_IPV4_EMBED | IPV6_FLAG_IPV4_COMPAT);
        num_components = IPV6_NUM_COMPONENTS;
        a_components = &a->address.components[0];
        b_components = &b->address.components[0];
    }

    // Make sure desired features are the same
    if ((a->flags & compare_flags) != (b->flags & compare_flags)) {
        return IPV6_COMPARE_FORMAT_MISMATCH;
    }

    // Compare the desired number of components in order
    for (uint32_t i = 0; i < num_components; ++i) {
        if (a_components[i] != b_components[i]) {
            return IPV6_COMPARE_ADDRESS_MISMATCH;
        }
    }

    // Compare port
    if (   ((ignore_flags & IPV6_FLAG_HAS_PORT) == 0)
        && ((a->flags & IPV6_FLAG_HAS_PORT) || (b->flags & IPV6_FLAG_HAS_PORT)))
    {
        if (a->port != b->port) {
            return IPV6_COMPARE_PORT_MISMATCH;
        }
    }

    // Compare mask
    if (   ((ignore_flags & IPV6_FLAG_HAS_MASK) == 0)
        && ((a->flags & IPV6_FLAG_HAS_MASK) || (b->flags & IPV6_FLAG_HAS_MASK)))
    {
        if (a->mask != b->mask) {
            return IPV6_COMPARE_MASK_MISMATCH;
        }
    }

    return IPV6_COMPARE_OK;
}
