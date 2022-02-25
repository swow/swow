# PHP extension stub generator

Generate a single PHP source file from the extension's arginfo.

It can work with php-src's gen_stub.php to update arginfo of extension C source file automatically.

## Usage

### gen-stub.php

```
Usage: php gen-stub.php \
         [-h|--help] [--noinspection] [--stub-file=/path/to/ext.stub.php] [--gen-arginfo-mode] \
         [--filter-mode] [--function-filter=functionA|functionB] [--class-filter=classA|classB] \
         <extension-name> [output-target]
```

### update-arginfo.php

```
Usage: php update-arginfo.php \
         [--clear-cache] [--cache-path=/path/to/cache] [--stub-file=/path/to/ext.stub.php] \
         <extension-name> <extension-source-path> <extension-build-dir>
```

## Results show

+ [Swow](https://github.com/swow/swow/blob/develop/lib/swow-stub/src/Swow.php)

## License

This package is licensed using the MIT License.
