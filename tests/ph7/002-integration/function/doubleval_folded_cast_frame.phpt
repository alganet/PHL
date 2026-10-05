--TEST--
doubleval() is floatval()'s alias: a direct call has no frame of its own
--FILE--
<?php
// php compiles a direct one-argument doubleval() to the same cast opcode as
// floatval(), so the warning's handler runs from the USER frame with no
// doubleval frame between. Through a callback it is a real internal call and
// keeps exactly one frame — no frame for a body of its own inside it.
class S {}
function h($n, $s, $f, $l) {
    echo "handler: $s (line $l)\n";
    foreach (debug_backtrace() as $fr) {
        echo '  ', isset($fr['file']) ? 'line ' . $fr['line'] : '[internal]', ' ', $fr['function'], "\n";
    }
    return true;
}
set_error_handler('h');
function g() { return doubleval(new S); }
var_dump(g());
var_dump(floatval(new S));
var_dump(array_map('doubleval', [new S]));
var_dump(array_map('floatval', [new S]));
$r = new ReflectionFunction('doubleval');
var_dump($r->isInternal(), $r->getFileName(), $r->getExtensionName());
echo $r;
var_dump(doubleval(value: '3.5'), doubleval(...)('4'));
try { doubleval(1, 2); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
handler: Object of class S could not be converted to float (line 15)
  line 15 h
  line 16 g
float(1)
handler: Object of class S could not be converted to float (line 17)
  line 17 h
float(1)
handler: Object of class S could not be converted to float (line 18)
  [internal] h
  [internal] doubleval
  line 18 array_map
array(1) {
  [0]=>
  float(1)
}
handler: Object of class S could not be converted to float (line 19)
  [internal] h
  [internal] floatval
  line 19 array_map
array(1) {
  [0]=>
  float(1)
}
bool(true)
bool(false)
string(8) "standard"
Function [ <internal:standard> function doubleval ] {

  - Parameters [1] {
    Parameter #0 [ <required> mixed $value ]
  }
  - Return [ float ]
}
float(3.5)
float(4)
doubleval() expects exactly 1 argument, 2 given
--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
