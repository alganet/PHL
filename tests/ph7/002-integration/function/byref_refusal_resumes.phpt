--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A caught "could not be passed by reference" Error resumes the enclosing scope
--FILE--
<?php
function r(&$x) { $x = 'W'; return 'ok'; }
function r2($a, &$x) { $x = 'W'; return 'ok'; }

echo "== top-level statement ==\n";
try { r(1 + 1); } catch (Throwable $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "after top-level\n";

echo "== inside a function ==\n";
function w() { r(1 + 1); return 'unreached'; }
try { var_dump(w()); } catch (Throwable $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "after function\n";

echo "== inside a closure ==\n";
$c = function () { r(1 + 1); return 'unreached'; };
try { var_dump($c()); } catch (Throwable $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "after closure\n";

echo "== named-argument form ==\n";
try { r2(x: 1 + 1, a: 5); } catch (Throwable $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "after named\n";

echo "== mid-expression, repeatedly ==\n";
for ($i = 0; $i < 2000; $i++) {
    try { $v = 'x' . r(1 + 1); } catch (Throwable $e) { $v = 'caught'; }
}
echo $v, " after ", $i, " rounds\n";

echo "== the refused call produces no value and runs no body ==\n";
try { $out = r(1 + 1); } catch (Throwable $e) { echo get_class($e), "\n"; }
var_dump(isset($out));
echo "done\n";
?>
--EXPECT--
== top-level statement ==
caught: r(): Argument #1 ($x) could not be passed by reference
after top-level
== inside a function ==
caught: r(): Argument #1 ($x) could not be passed by reference
after function
== inside a closure ==
caught: r(): Argument #1 ($x) could not be passed by reference
after closure
== named-argument form ==
caught: r2(): Argument #2 ($x) could not be passed by reference
after named
== mid-expression, repeatedly ==
caught after 2000 rounds
== the refused call produces no value and runs no body ==
Error
bool(false)
done
