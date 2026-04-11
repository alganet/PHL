--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: reading uninitialized throws Error
--FILE--
<?php
class TpiLazy { public int $n; }
$l = new TpiLazy();
try {
    echo $l->n, "\n";
} catch (Error $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
$l->n = 7;
echo $l->n, "\n";
?>
--EXPECT--
caught: Typed property TpiLazy::$n must not be accessed before initialization
7
--CLEAN--
<?php
unset($l);
