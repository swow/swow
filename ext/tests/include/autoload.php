<?php
/* TODO: can we go to PHPUnit? */
$autoloader = __DIR__ . '/../../../vendor/autoload.php';
if (file_exists($autoloader)) {
    require $autoloader;
}
