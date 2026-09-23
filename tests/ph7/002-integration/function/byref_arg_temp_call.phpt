--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A call result in a by-reference position is php's notice, and the callee operates on it
--FILE--
<?php
error_reporting(E_ALL);
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

function makeArray() { return [3, 1, 2]; }
function &byRefReturn() { static $v = [3, 1, 2]; return $v; }
function r(&$x) { $x = 'W'; return 'ok'; }
class C {
    public function m() { return [3, 1, 2]; }
    public static function sm() { return [3, 1, 2]; }
}

echo "== a builtin operates on the temporary ==\n";
var_dump(sort(makeArray()));
var_dump(array_pop(makeArray()));
var_dump(preg_match('/a/', 'a', makeArray()));

echo "== every call-ish shape notices ==\n";
var_dump(sort((new C)->m()));
var_dump(sort(C::sm()));
var_dump(sort((function () { return [3, 1, 2]; })()));

echo "== a user function's by-ref parameter too ==\n";
var_dump(r(makeArray()));
var_dump(r(new C));

echo "== a by-REFERENCE return is silent, and binds ==\n";
var_dump(sort(byRefReturn()));
var_dump(byRefReturn());

echo "== the notice does not swallow the TYPE error ==\n";
try { sort(new C); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== a LITERAL is still the Error, not a notice ==\n";
try { sort([3, 1]); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
?>
--EXPECT--
== a builtin operates on the temporary ==
  [8] Only variables should be passed by reference
bool(true)
  [8] Only variables should be passed by reference
int(2)
  [8] Only variables should be passed by reference
int(1)
== every call-ish shape notices ==
  [8] Only variables should be passed by reference
bool(true)
  [8] Only variables should be passed by reference
bool(true)
  [8] Only variables should be passed by reference
bool(true)
== a user function's by-ref parameter too ==
  [8] Only variables should be passed by reference
string(2) "ok"
  [8] Only variables should be passed by reference
string(2) "ok"
== a by-REFERENCE return is silent, and binds ==
bool(true)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
== the notice does not swallow the TYPE error ==
  [8] Only variables should be passed by reference
TypeError: sort(): Argument #1 ($array) must be of type array, C given
== a LITERAL is still the Error, not a notice ==
Error: sort(): Argument #1 ($array) could not be passed by reference
