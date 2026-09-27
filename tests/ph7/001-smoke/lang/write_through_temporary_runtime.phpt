--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A write THROUGH a temporary is performed and discarded, php's way
--FILE--
<?php
// php compiles these, writes into the value the call answered and drops it with
// the temporary -- saying nothing. PHL announced every one of them with
// "Cannot perform assignment on a constant class attribute[,PH7 is loading
// NULL]", a diagnostic php has no equivalent of, and skipped php's own screen on
// the base's TYPE along with the write.

class WttHolder { public $p = 0; public $q = 'a'; }
function wttArr()  { return [1, 2, 3]; }
function wttNew()  { return new WttHolder; }
function wttInt()  { return 7; }
function wttNull() { return null; }
function wttStr()  { return "ab"; }
function wttBool() { return true; }
function wttNest() { return [[1, 2]]; }

// Silent: the store lands on the temporary and dies with it.
wttArr()[0] = 5;
wttArr()[0] += 5;
wttArr()[0] .= "x";
wttArr()[0] *= 2;
wttArr()[0] **= 2;
wttArr()[0] |= 1;
wttArr()[0] <<= 1;
wttArr()[0] %= 2;
wttArr()[0]++;
--wttArr()[0];
wttNew()->p = 5;
wttNew()->p += 5;
wttNew()->q .= "b";
wttNew()->p++;
unset(wttNew()->p);
wttNull()[0] = 5;
wttStr()[0] = "z";
wttNest()[0][1] = 5;
echo "silent ok\n";

// A fresh object really is mutated -- it is just unreachable afterwards.
function wttSame() { static $h; if (!$h) { $h = new WttHolder; } return $h; }
wttSame()->p = 5;
echo "retained: ", wttSame()->p, "\n";
echo "fresh: ", wttNew()->p, "\n";

// php screens the base's TYPE the same way whether or not there is a slot
// behind it: an int, a float, a resource or a bool base is its catchable Error.
foreach (['int' => 'wttInt', 'bool' => 'wttBool'] as $label => $fn) {
    try {
        $fn()[0] = 5;
        echo "$label: no error\n";
    } catch (Throwable $e) {
        echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}
try {
    wttArr()[0][1] = 5;
} catch (Throwable $e) {
    echo "nested: ", get_class($e), ": ", $e->getMessage(), "\n";
}
try {
    wttInt()[0] += 5;
} catch (Throwable $e) {
    echo "compound: ", get_class($e), ": ", $e->getMessage(), "\n";
}

// The same verdicts on an ordinary variable, unchanged.
$wttV = 7;
try {
    $wttV[0] = 5;
} catch (Throwable $e) {
    echo "variable: ", get_class($e), ": ", $e->getMessage(), "\n";
}
var_dump($wttV);
$wttN = null;
$wttN[0] = 5;
var_dump($wttN);
echo "END\n";
?>
--EXPECT--
silent ok
retained: 5
fresh: 0
int: Error: Cannot use a scalar value as an array
bool: Error: Cannot use a scalar value as an array
nested: Error: Cannot use a scalar value as an array
compound: Error: Cannot use a scalar value as an array
variable: Error: Cannot use a scalar value as an array
int(7)
array(1) {
  [0]=>
  int(5)
}
END
