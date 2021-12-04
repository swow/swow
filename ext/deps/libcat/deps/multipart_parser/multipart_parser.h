/* Based on node-formidable by Felix Geisendörfer
 * Igor Afonov - afonov@gmail.com - 2012
 * MIT License - http://www.opensource.org/licenses/mit-license.php
 */
#ifndef _multipart_parser_h
#define _multipart_parser_h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

typedef struct multipart_parser multipart_parser;
typedef struct multipart_parser_settings multipart_parser_settings;
typedef struct multipart_parser_state multipart_parser_state;

enum multipart_error
{
    MPPE_OK = 0,
    MPPE_PAUSED,
    MPPE_UNKNOWN,
    MPPE_BOUNDARY_END_NO_CRLF,
    MPPE_BAD_START_BOUNDARY,
    MPPE_INVALID_HEADER_FIELD_CHAR,
    MPPE_INVALID_HEADER_VALUE_CHAR,
    MPPE_BAD_PART_END,
    MPPE_END_BOUNDARY_NO_DASH,
};

#ifdef SIZE_MAX
#   define MPPE_ERROR SIZE_MAX
#else
#   define MPPE_ERROR ((size_t)-1)
#endif

/*
* calloc function used in multipart_parser_alloc
*/
typedef void* (*multipart_calloc_func)(size_t size, size_t count);
/*
* free function used in multipart_parser_free
*/
typedef void (*multipart_free_func)(void *ptr);

/*
* data callback called at events, return MPE_OK to continue parser,
* MPE_PAUSED for making parser paused
*/
typedef long (*multipart_data_cb) (multipart_parser*, const char *at, size_t length);
/*
* notification callback called at events, return MPE_OK to continue parser,
* MPE_PAUSED for making parser paused
*/
typedef long (*multipart_notify_cb) (multipart_parser*);

// from RFC2046
#define BOUNDARY_MAX_LEN 70

struct multipart_parser {
    /* private holder for callbacks */
    const multipart_parser_settings* settings;
    /* private internal index for matching boundary */
    size_t index;
    /* public error unexpected char index */
    size_t error_i;
    /* private boundary length + 2 ("--") */
    unsigned char boundary_length;
    /* private FSM state */
    unsigned char state;
    /* public error reason */
    unsigned char error_reason;
    /* private boundary storage: "--" + boundary */
    char multipart_boundary[(2 + BOUNDARY_MAX_LEN)*2 + 9];
    /* public error expected char */
    char error_expected;
    /* public error unexpected char */
    char error_unexpected;
};

struct multipart_parser_settings {
    /* used in multipart_parser_alloc for allocating parser buffer */
    multipart_calloc_func calloc;
    /* used in ultipart_parser_free for free parser allocated in ultipart_parser_alloc */
    multipart_free_func free;

    /*
    * data callback called on header field coming
    * for example data is "Content-Type" with length 12
    */
    multipart_data_cb on_header_field;
    /*
    * data callback called on header value coming
    * for example data is "plain/text" with length 10
    */
    multipart_data_cb on_header_value;
    /*
    * data callback called on body data coming
    */
    multipart_data_cb on_part_data;

    /*
    * before "--" boundary
    */
    multipart_notify_cb on_part_data_begin;
    /*
    * after all headers line "\r\n", before body
    */
    multipart_notify_cb on_headers_complete;
    /*
    * after body, before next "--" boundary
    */
    multipart_notify_cb on_part_data_end;
    /*
    * after last "--" boundary "--"
    */
    multipart_notify_cb on_body_end;
};

/*
*   initialize multipart parser
*   @param mp pointer to mp struct
*   @param boundary boundary string
*   @param boundary_length boundary length in bytes, max length is 70
*   @param settings settings structure used, if it setted to NULL, keep current
*   @return 0 for success, -1 for failed, errno is setted
*/
int multipart_parser_init(
    multipart_parser *mp,
    const char *boundary,
    size_t boundary_length,
    const multipart_parser_settings* settings);
/*
*   allocate multipart parser
*   @param boundary boundary string
*   @param boundary_length boundary length in bytes, max length is 70
*   @param settings settings structure used
*   @return pointer to allocated memory for success, NULL for failed, errno is setted
*/
multipart_parser* multipart_parser_alloc(
    const char *boundary,
    size_t boundary_length,
    const multipart_parser_settings* settings);

/*
*   free multipart parser allocated by multipart_parser_alloc
*   @param p multipart parser to be free'd
*/
void multipart_parser_free(multipart_parser* p);

/*
*   execute data buf with size len
*   @param p multipart parser to be used
*   @param buf buffer for consuming
*   @param len buffer size
*   @return parsed length in bytes, MPPE_ERROR for error
*/
size_t multipart_parser_execute(multipart_parser* p, const char *buf, size_t len);


int multipart_parser_error_msg(multipart_parser* p, char *buf, size_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
