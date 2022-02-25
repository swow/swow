<?php

use function Swow\Debug\registerExtendedStatementHandler;

$handler = registerExtendedStatementHandler(function () {
echo "Handler\n";
});
echo "Remove Handler\n";
$handler->remove();
echo "I am gone\n";

echo "Done\n";
