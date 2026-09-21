--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A variadic type-error numbers by element position and omits the ($name)
--FILE--
<?php
// php reports a variadic-collected argument's type error by the offending
// element's overall 1-based CALL position (not the formal index) and OMITS the
// ` ($name)` clause (many values share the one variadic formal). A plain
// non-variadic parameter keeps its name. Covers scalar, object, and union
// variadic hints, a leading fixed parameter, and a method.
function shortMsg(TypeError $e) {
    $msg = $e->getMessage();
    $pos = strpos($msg, ", called in");
    if ($pos !== false) $msg = substr($msg, 0, $pos);
    return $msg;
}

function vArray(array ...$a) {}
function vInt(int ...$a) {}
function vObject(object ...$a) {}
function vUnion(int|string ...$a) {}
function vLead(int $x, string $y, array ...$a) {}
class Svc { function handle(array ...$rows) {} }

try { vArray([1], 2);            } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { vArray(9);                 } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { vArray([1], [2], 3);       } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { vInt(1, "abc");            } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { vObject(new stdClass, 5);  } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { vUnion(1, [2]);            } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { vLead(0, "s", [1], 2);     } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { (new Svc)->handle([1], 2); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// A non-variadic parameter still names the argument.
function fixed(int $x, array $a) {}
try { fixed(1, 5); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// A valid variadic call still binds every element.
vArray([1], [2], [3]);
echo "ok\n";
?>
--EXPECT--
vArray(): Argument #2 must be of type array, int given
vArray(): Argument #1 must be of type array, int given
vArray(): Argument #3 must be of type array, int given
vInt(): Argument #2 must be of type int, string given
vObject(): Argument #2 must be of type object, int given
vUnion(): Argument #2 must be of type string|int, array given
vLead(): Argument #4 must be of type array, int given
Svc::handle(): Argument #2 must be of type array, int given
fixed(): Argument #2 ($a) must be of type array, int given
ok
--CLEAN--
<?php
