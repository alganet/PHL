--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
new class extends an autoloaded parent (autoloader registered in the same file)
--DESCRIPTION--
Anonymous classes compile inline at the enclosing file's compile
time, before spl_autoload_register() has run. The declaration defers to its
execution point like php's. One class per source site (loop identity stays
stable) and constructor arguments still bind, including promoted properties.
--FILE--
<?php
spl_autoload_register(function ($cls) {
    if ($cls === 'Dep\AnonBase') {
        eval('namespace Dep; class AnonBase { public function base() { return "base"; } }');
    }
});
$o = new class extends \Dep\AnonBase {
    public function hello() { return "hello from anon"; }
};
echo $o->hello(), " / ", $o->base(), "\n";
$names = [];
for ($i = 0; $i < 3; $i++) {
    $a = new class(10 + $i) extends \Dep\AnonBase {
        public function __construct(public int $v) {}
    };
    $names[] = get_class($a);
    echo $a->v, " ";
}
echo count(array_unique($names)) === 1 ? "one-class-per-site" : "unstable", "\n";
echo $o instanceof \Dep\AnonBase ? "instanceof-ok" : "no", "\n";
?>
--EXPECT--
hello from anon / base
10 11 12 one-class-per-site
instanceof-ok
--CLEAN--
<?php
