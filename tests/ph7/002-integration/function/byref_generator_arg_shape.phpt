--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator's by-reference parameter refuses a non-variable at the g(...) call
--FILE--
<?php
error_reporting(E_ALL);
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

function g(&$x, $y = 1) { $x = 'W'; yield 1; }
class K { public function m(&$x) { $x = 'W'; yield 1; } }
function t($label, $fn) {
    try { $fn(); echo "$label: built\n"; }
    catch (Throwable $e) { echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

echo "== refused, before the Generator object exists ==\n";
t('literal',   fn() => g(1 + 1));
t('named',     fn() => g(y: 2, x: 1 + 1));
t('method',    fn() => (new K)->m(1 + 1));

echo "== a variable still binds ==\n";
t('variable',  function () { $v = 1; return g($v); });
t('named var', function () { $v = 1; return g(y: 2, x: $v); });
t('element',   function () { $a = ['k' => 1]; return g($a['k']); });

echo "== a call result is the notice, and it builds ==\n";
t('call',      fn() => g(strtoupper('a')));
?>
--EXPECT--
== refused, before the Generator object exists ==
literal: Error: g(): Argument #1 ($x) could not be passed by reference
named: Error: g(): Argument #1 ($x) could not be passed by reference
method: Error: K::m(): Argument #1 ($x) could not be passed by reference
== a variable still binds ==
variable: built
named var: built
element: built
== a call result is the notice, and it builds ==
  [8] Only variables should be passed by reference
call: built
