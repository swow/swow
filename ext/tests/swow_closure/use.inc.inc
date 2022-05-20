<?php

namespace A {
    class B
    {
        function __construct()
        {
            echo basename(__FILE__) . ": class B:__construct\n";
        }
    }
    class C
    {
        function __construct()
        {
            echo basename(__FILE__) . ": class C:__construct\n";
        }
    }
    class D
    {
        function __construct()
        {
            echo basename(__FILE__) . ": class D:__construct\n";
        }
    }
    function E()
    {
        echo basename(__FILE__) . ": func E\n";
    }
    function F()
    {
        echo basename(__FILE__) . ": func F\n";
    }
    const G = 'const G';
    const H = 'const H';
    class I
    {
        function __construct()
        {
            echo basename(__FILE__) . ": class I:__construct\n";
        }
    }
    class J
    {
        function __construct()
        {
            echo basename(__FILE__) . ": class J:__construct\n";
        }
    }
    class K
    {
        function __construct()
        {
            echo basename(__FILE__) . ": class K:__construct\n";
        }
    }
}

namespace A\L\M {
    class N
    {
        function __construct()
        {
            echo basename(__FILE__) . ": class N:__construct\n";
        }
    }
    class O
    {
        function __construct()
        {
            echo basename(__FILE__) . ": class O:__construct\n";
        }
    }
    function P()
    {
        echo basename(__FILE__) . ": func P\n";
    }
    function Q()
    {
        echo basename(__FILE__) . ": func Q\n";
    }
    const R = 'const R';
    const S = 'const S';
    const T = 'const T';
    function U()
    {
        echo basename(__FILE__) . ": func U\n";
    }
    class V
    {
        function __construct()
        {
            echo basename(__FILE__) . ": class V:__construct\n";
        }
    }
    /*
    const W = 'const W';
    function W () {
        echo basename(__FILE__) . ": func W\n";
    }
    */
}
