<?php

declare(strict_types=1);

namespace A {
    function getNamespacedClosureInNamespacedClosure()
    {
        echo __LINE__ . ':' . __NAMESPACE__ . ' ';
        return static function () {
            echo __LINE__ . ':' . __NAMESPACE__ . ' ';
            return static function (): void {
                echo __LINE__ . ':' . __NAMESPACE__ . "\n";
            };
        };
    }

    class A
    {
        public static function getAnonymousInNamespacedClass()
        {
            echo __LINE__ . ':' . __NAMESPACE__ . ' ';
            return static function () {
                echo __LINE__ . ':' . __NAMESPACE__ . ' ';
                return static function (): void {
                    echo __LINE__ . ':' . __NAMESPACE__ . "\n";
                };
            };
        }
    }
}

namespace B {
    function getNamespacedClosureInAnotherNamespacedClosure()
    {
        echo __LINE__ . ':' . __NAMESPACE__ . ' ';
        return static function () {
            echo __LINE__ . ':' . __NAMESPACE__ . ' ';
            return \A\getNamespacedClosureInNamespacedClosure();
        };
    }

    class A
    {
        public static function getAnonymousInAnotherNamespacedClass()
        {
            echo __LINE__ . ':' . __NAMESPACE__ . ' ';
            return static function () {
                echo __LINE__ . ':' . __NAMESPACE__ . ' ';
                return \A\A::getAnonymousInNamespacedClass();
            };
        }
    }
}
