--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Closure is callable (is_callable + callable type-hint)
--FILE--
<?php
$f = fn() => 42;
echo var_export(is_callable($f), true), "\n";
function cic_run(callable $c){ return $c(); }
echo cic_run($f), "\n";
echo cic_run(function(){ return "hi"; }), "\n";
?>
--EXPECT--
true
42
hi
--CLEAN--
<?php
