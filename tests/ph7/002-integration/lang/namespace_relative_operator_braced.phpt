--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
namespace\ in BRACKETED namespaces, and at global scope where it means \X
--FILE--
<?php
namespace A {

    class Cee { const K = 'A\Cee'; }
    function eff() { return 'A\eff'; }

    echo namespace\Cee::K, '|', namespace\eff(), "\n";
    echo get_class(new namespace\Cee), "\n";
}

namespace Z {

    class Cee { const K = 'Z\Cee'; }
    function eff() { return 'Z\eff'; }

    // Same spelling, different block: the operator follows the ACTIVE namespace.
    echo namespace\Cee::K, '|', namespace\eff(), "\n";
}

namespace {

    class Gee { const K = 'global Gee'; }
    function gee() { return 'global gee'; }

    // At global scope a relative name is just the global name.
    echo namespace\Gee::K, '|', namespace\gee(), "\n";
    echo namespace\strlen('abc'), '|', namespace\PHP_EOL;
    var_dump(new namespace\Gee instanceof \Gee);
}
?>
--EXPECT--
A\Cee|A\eff
A\Cee
Z\Cee|Z\eff
global Gee|global gee
3|
bool(true)
--CLEAN--
<?php
