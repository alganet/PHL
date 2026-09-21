--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_callable()'s &$callable_name names an anonymous closure {closure:file:line}
--FILE--
<?php
// A real anonymous closure is named by where it was WRITTEN, not by the
// synthesized internal key — even after bindTo(), which changes $this, not the
// underlying function.
$anon = function () {};
$n = 'PRESET';
is_callable($anon, false, $n);
echo $n, "\n";
class ClosureNameHost { public $p = 1; }
$bound = $anon->bindTo(new ClosureNameHost());
$n = 'PRESET';
is_callable($bound, false, $n);
echo $n, "\n";
// A first-class callable over a host function keeps the plain function name.
$fcc = strlen(...);
$n = 'PRESET';
is_callable($fcc, false, $n);
echo $n, "\n";
?>
--EXPECTF--
{closure:%s:5}
{closure:%s:5}
strlen
--CLEAN--
<?php
