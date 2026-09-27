--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A prefix unary covers the whole assignment, php's binding
--FILE--
<?php
// php parses `-$x = 5` as `-($x = 5)`: the write happens and the operator is
// applied to its RESULT. Only the `@` spelling was handled here, so the other
// eight -- `-`, `+`, `!`, `~` and the five casts -- did not compile at all.

$uoaI = 1;
var_dump(-$uoaI = 5, $uoaI);
var_dump(+$uoaI = 6, $uoaI);
var_dump(!$uoaI = 7, $uoaI);
var_dump(~$uoaI = 8, $uoaI);
var_dump(@$uoaI = 9, $uoaI);

// Casts bind looser than the assignment too: the VARIABLE keeps the raw value
// and only the expression's result is cast.
$uoaC = 0;
var_dump((int)$uoaC = "9", $uoaC);
var_dump((string)$uoaC = 9, $uoaC);
var_dump((bool)$uoaC = 0, $uoaC);
var_dump((float)$uoaC = 3, $uoaC);
$uoaO = new stdClass;
var_dump((array)$uoaO = 5, $uoaO);

// Chained, and over a compound assign, a subscript, a property and a bind.
$uoaN = 1;
var_dump(- -$uoaN = 4, $uoaN);
var_dump(!!$uoaN = 5, $uoaN);
var_dump(-(int)$uoaN = 6, $uoaN);
$uoaN = 1;
var_dump(-$uoaN += 5, $uoaN);
$uoaA = [1, 2];
var_dump((int)$uoaA[0] = 7, $uoaA[0]);
class UoaHolder { public $p = 0; }
$uoaH = new UoaHolder;
var_dump((int)$uoaH->p = 8, $uoaH->p);
$uoaSrc = 4;
-$uoaA[1] =& $uoaSrc;
$uoaSrc = 11;
var_dump($uoaA[1]);

// A PARENTHESISED operand is a genuine non-lvalue, and `new` keeps php's own
// production; both stay refused (checked in 002-integration/parse).
echo "END\n";
?>
--EXPECT--
int(-5)
int(5)
int(6)
int(6)
bool(false)
int(7)
int(-9)
int(8)
int(9)
int(9)
int(9)
string(1) "9"
string(1) "9"
int(9)
bool(false)
int(0)
float(3)
int(3)
array(1) {
  [0]=>
  int(5)
}
int(5)
int(4)
int(4)
bool(true)
int(5)
int(-6)
int(6)
int(-6)
int(6)
int(7)
int(7)
int(8)
int(8)
int(11)
END
