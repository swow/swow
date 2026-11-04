## Multipart form data parser

### Modify

This version of multipart-parser-c is modified for llhttp PAUSE mode, the original `MPPE_OK` mode may not work (It may work but was not tested).

This version is tested by simple fuzz with honggfuzz, used in [swow](https://github.com/swow/swow) and [swoole](https://github.com/swoole/swoole-src).

### Features
* No dependencies
* Works with chunks of a data - no need to buffer the whole request
* Almost no internal buffering. Buffer size doesn't exceed the size of the boundary (~60-70 bytes)

Tested as part of [Cosmonaut](https://github.com/iafonov/cosmonaut) HTTP server.

Implementation based on [node-formidable](https://github.com/felixge/node-formidable) by [Felix Geisendörfer](https://github.com/felixge).

Inspired by [http-parser](https://github.com/joyent/http-parser) by [Ryan Dahl](https://github.com/ry).

### Usage (C)
This parser library works with several callbacks, which the user may set up at application initialization time.

```c
multipart_parser_settings callbacks;

memset(&callbacks, 0, sizeof(multipart_parser_settings));

callbacks.on_header_field = read_header_name;
callbacks.on_header_value = read_header_value;
```

These functions must match the signatures defined in the multipart-parser header file.  For this simple example, we'll just use two of the available callbacks to print all headers the library finds in multipart messages.

The data callbacks receive a pointer into the current buffer and a length; the pointer is only valid until the callback returns, so a consumer that keeps the data must copy it.  Return `MPPE_OK` to continue parsing, `MPPE_PAUSED` to pause after the current event, or `MPPE_ERROR` to abort.

```c
long read_header_name(multipart_parser* p, const char *at, size_t length)
{
   printf("%.*s: ", (int)length, at);
   return MPPE_OK;
}

long read_header_value(multipart_parser* p, const char *at, size_t length)
{
   printf("%.*s\n", (int)length, at);
   return MPPE_OK;
}
```

When a message arrives, callers must parse the multipart boundary from the **Content-Type** header (see the [RFC](http://tools.ietf.org/html/rfc2387#section-5.1) for more information and examples), then feed every received chunk to the parser.

The parser can be allocated on the heap:

```c
multipart_parser* parser = multipart_parser_alloc(boundary, boundary_length, &callbacks);
if (parser == NULL) {
   /* errno is set */
   return -1;
}

size_t parsed = multipart_parser_execute(parser, body, length);
if (parsed == MPPE_ERROR) {
   multipart_parser_error_msg(parser, msg, sizeof(msg));
}

multipart_parser_free(parser);
```

or placed on the stack:

```c
multipart_parser parser;

if (multipart_parser_init(&parser, boundary, boundary_length, &callbacks) != 0) {
   /* errno is set */
   return -1;
}

multipart_parser_execute(&parser, body, length);
```

`multipart_parser_execute()` returns the number of consumed bytes, or `MPPE_ERROR`.  In PAUSE mode it also returns early when a callback returns `MPPE_PAUSED`, so the caller resumes parsing with the remaining buffer at the returned offset:

```c
size_t offset = 0;
while (offset < length) {
   size_t parsed = multipart_parser_execute(&parser, body + offset, length - offset);
   if (parsed == MPPE_ERROR) {
      break;
   }
   if (parsed == 0) {
      break; /* need more data */
   }
   offset += parsed;
}
```

A boundary longer than `BOUNDARY_MAX_LEN` (70) bytes is rejected with `errno = EINVAL`.  Passing `NULL` as the boundary to `multipart_parser_init()` keeps the boundary of a previously initialised parser; `multipart_parser_alloc()` needs a real boundary.

### Fuzzing

`test/multipart_fuzz.c` is a [honggfuzz](https://github.com/google/honggfuzz) target. It feeds one body through several feed schedules, plain and with callbacks that pause on every event, checks that every callback span points inside the input of the current `execute()` call, and that a body that parses in one call parses the same way when split at any point.

Build honggfuzz first, then run a session:

```sh
make fuzz HONGGFUZZ_DIR=~/honggfuzz
```

`FUZZ_RUN_TIME` limits the session length in seconds (the default runs until interrupted), `FUZZ_THREADS` sets the number of workers (default 2), and `FUZZ_MAX_INPUT` the maximum input size (default 4096). Seed inputs are `test/hello_world.data` and `test/fuzz-corpus/*.bin`; new coverage inputs and crash files go to `test/out/fuzz/`.

### Contributors
* [Daniel T. Wagner](http://www.danieltwagner.de/)
* [James McLaughlin](http://udp.github.com/)
* [Jay Miller](http://www.cryptofreak.org)

© 2012 [Igor Afonov](http://iafonov.github.com)
