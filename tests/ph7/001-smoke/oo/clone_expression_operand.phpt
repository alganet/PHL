--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
clone takes any expression as its operand, and refuses a non-object at run time
--FILE--
<?php
class CloneOperandQ {
    public $n = 1;
    public function __clone() { echo "[cloned]"; }
}
$q = new CloneOperandQ;
$arr = ['k' => $q];
$mk = fn() => new CloneOperandQ;

// `new` without a constructor-argument list: the defensive-copy spelling
$a = clone new CloneOperandQ;
echo " ", get_class($a), "\n";
$b = clone new CloneOperandQ();
echo " ", get_class($b), "\n";

// a clone of a clone
$c = clone clone $q;
echo " ", get_class($c), "\n";

// inside a literal, and through the shapes that already worked
echo get_class([clone new CloneOperandQ][0]), "\n";
echo get_class(clone $mk()), "\n";
echo get_class(clone $arr['k']), "\n";
echo get_class(clone match (1) { 1 => $q }), "\n";
echo get_class(clone (true ? $q : $q)), "\n";

// a non-object operand is a runtime TypeError, not a compile error
foreach ([5, "s", [], null, true, 1.5] as $bad) {
    try { clone $bad; } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
try { clone []; } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { clone null; } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

// clone binds tighter than the binary operators
var_dump(clone $q instanceof CloneOperandQ);
$d = clone $q;
var_dump($d->n + 1);
?>
--EXPECT--
[cloned] CloneOperandQ
[cloned] CloneOperandQ
[cloned][cloned] CloneOperandQ
[cloned]CloneOperandQ
[cloned]CloneOperandQ
[cloned]CloneOperandQ
[cloned]CloneOperandQ
[cloned]CloneOperandQ
clone(): Argument #1 ($object) must be of type object, int given
clone(): Argument #1 ($object) must be of type object, string given
clone(): Argument #1 ($object) must be of type object, array given
clone(): Argument #1 ($object) must be of type object, null given
clone(): Argument #1 ($object) must be of type object, true given
clone(): Argument #1 ($object) must be of type object, float given
clone(): Argument #1 ($object) must be of type object, array given
clone(): Argument #1 ($object) must be of type object, null given
[cloned]bool(true)
[cloned]int(2)
