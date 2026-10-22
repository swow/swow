# fastext PHP demo

This example implements the fastext blob format in PHP.
It uses the Swow process manager to run parallel archive and extract workers.

## Format

```text
uint64 metadata_offset
data region
metadata groups
```

The metadata offset uses big-endian byte order.
The data region starts at offset 8.
Each file has its own compressed or raw data range.

## Commands

```bash
php examples/fastext/fastext.php archive \
  --source=/home/dixyes/php/php-src \
  --blob=/home/dixyes/php/php-src.blob \
  --workers=8 \
  --mode=inline \
  --compression=auto

php examples/fastext/fastext.php extract \
  --blob=/home/dixyes/php/php-src.blob \
  --dest=/home/dixyes/php/php-src-out \
  --workers=8 \
  --mode=inline

php examples/fastext/fastext.php verify \
  --source=/home/dixyes/php/php-src \
  --dest=/home/dixyes/php/php-src-out
```

## Modes

- `fork`: uses `ForkProcessManager`. POSIX only.
- `inline`: uses `ProcessManager::startInline`.
- `script`: uses `ProcessManager::startScript`.

## Compression

- `auto`: use zstd if the zstd stream filter is available, otherwise zlib
- `zstd`: require the zstd stream filter
- `zlib`: use zlib with the PHP-only `NodeCompressZlib` flag
- `none`: store raw data

The PHP-only zlib flag is not compatible with the Go fastext reader.

## Windows timestamps

On Windows, the extractor uses FFI and the Win32 `CreateFileW` / `SetFileTime`
API to restore modification times. If FFI is not available, the extractor
prints one warning and skips timestamp restoration.
