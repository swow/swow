<?php declare(strict_types=1);

$anonymous = static function (): void {
    ?>hello world
<?php ?>!!!<?php
    echo "anonymous\n";
};
