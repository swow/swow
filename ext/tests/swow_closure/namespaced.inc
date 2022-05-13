<?php

namespace SomeNamespace;

function getAnonymous()
{
    return function () {
        echo ('im namespaced! ' . basename(__FILE__) . ':' . __NAMESPACE__) . "\n";
    };
}
