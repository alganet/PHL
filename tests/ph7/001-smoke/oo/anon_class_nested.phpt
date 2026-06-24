--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: nested anonymous class in a method
--FILE--
<?php
$o = new class {
    function make() { return new class { function g() { return 42; } }; }
};
echo $o->make()->g(), "\n";
?>
--EXPECT--
42
--CLEAN--
<?php
unset($o);
