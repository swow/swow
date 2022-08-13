# Swow php-cs-fixer config

## Usage

```shell
composer require swow/swow-php-cs-fixer-config:dev-develop
```

```shell
touch .php-cs-fixer.dist.php
```

```php
<?php

use Swow\PhpCsFixer\Config;

require __DIR__ . '/vendor/autoload.php';

return (new Config())
    ->setHeaderComment(
        projectName: 'your project name',
        projectLink: 'your project link',
        contactName: 'your name',
        contactMail: 'your mail address'
    )->setFinder(
        PhpCsFixer\Finder::create()
            ->in([
                __DIR__ . '/src',
            ])
            ->append([
                __FILE__,
            ])
    );
```
