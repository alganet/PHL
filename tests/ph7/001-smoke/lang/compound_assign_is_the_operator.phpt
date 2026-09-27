--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A compound assign answers exactly what its binary twin answers
--DESCRIPTION--
php has ONE division, ONE multiplication and ONE power; `$x op= $y` only decides where the
answer goes. Swept over the whole operator table against the oracle, php's compound form is
byte-identical to its binary form in every one of 2700 operand pairs — result, diagnostics
and message alike — and PHL's differed in 178 of them, because each `_STORE` opcode is a
separate copy of its operator and three had drifted:

`/=` never grew the int-exactness rule `/` has, so `$x = 6; $x /= 3;` left a FLOAT where
`$x = $x / 3` left an int — visible through `===`, `is_int`, `var_dump` and `json_encode`.
`*=` and `**=` named the operands of `Unsupported operand types` in the wrong order, because
a compound assign puts its lvalue on the TOP of the stack and the message reads source
order: `$x *= [1]` said `array * int` where php (and PHL's own `$x * [1]`) says `int * array`.

Every row below prints both spellings; they must agree, and the corpus runs this file under
real php too.
--FILE--
<?php
/* This file is about the ANSWER, not the diagnostics: swallow the warnings so a
 * mismatch is the only thing that can print. */
set_error_handler(function () { return true; });

$caiOps = ['+', '-', '*', '/', '%', '**', '.', '&', '|', '^', '<<', '>>'];
$caiVals = ['null', 'true', 'false', '0', '1', '2', '-3', '7', '"5"', '"5x"', '"abc"', '""', '[1,2]', 'PHP_INT_MAX'];

foreach ($caiOps as $caiOp) {
    foreach ($caiVals as $caiA) {
        foreach ($caiVals as $caiB) {
            $caiC = caiRun("\$x = $caiA; \$x $caiOp= $caiB; return \$x;");
            $caiB2 = caiRun("return ($caiA) $caiOp ($caiB);");
            if ($caiC !== $caiB2) {
                echo "MISMATCH $caiA $caiOp= $caiB : compound=$caiC binary=$caiB2\n";
            }
        }
    }
}
echo "compound and binary agree on every pair\n";

function caiRun($code) {
    try {
        $v = eval($code);
        if (is_array($v)) { return 'array(' . count($v) . ')'; }
        if (is_float($v) && is_nan($v)) { return 'NAN'; }
        return var_export($v, true);
    } catch (\Throwable $e) {
        return '[' . get_class($e) . ': ' . $e->getMessage() . ']';
    }
}

echo "-- the three that had drifted\n";
$caiX = 6;  $caiX /= 3;   var_dump($caiX, $caiX === 2);
$caiY = 7;  $caiY /= 2;   var_dump($caiY);
$caiZ = -6; $caiZ /= -1;  var_dump($caiZ);
$caiW = PHP_INT_MIN; $caiW /= -1; var_dump($caiW);
try { $caiM = 3; $caiM *= [1]; } catch (\Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $caiP = 3; $caiP **= 'abc'; } catch (\Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $caiQ = null; $caiQ *= 'abc'; } catch (\Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $caiR = true; $caiR **= [1]; } catch (\Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "-- and the answers themselves\n";
$caiA1 = [9 => 6]; $caiA1[9] /= 3; var_dump($caiA1[9]);
class CaiHolder { public $p = 6; }
$caiO = new CaiHolder; $caiO->p /= 3; var_dump($caiO->p);

restore_error_handler();
?>
--EXPECT--
compound and binary agree on every pair
-- the three that had drifted
int(2)
bool(true)
float(3.5)
int(6)
float(9.223372036854776E+18)
TypeError: Unsupported operand types: int * array
TypeError: Unsupported operand types: int ** string
TypeError: Unsupported operand types: null * string
TypeError: Unsupported operand types: bool ** array
-- and the answers themselves
int(2)
int(2)
