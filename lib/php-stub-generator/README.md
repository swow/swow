# PHP extension stub generator

Generate a single PHP source file from the extension's arginfo.

It can work with php-src's gen_stub.php to update arginfo of extension C source file automatically.

## Usage

```
$ php gen-stub.php \
    [--stub-file=x.stub.php] [--gen-arginfo-mode] \
    [--function-filter=functionA|functionB] [--class-filter=classA|classB] \
    <extension_name> [output_path]
```

## Results show

+ [Swow](https://github.com/swow/swow/blob/develop/lib/swow-stub/Swow.php)

## License

This package is licensed using the MIT License.
