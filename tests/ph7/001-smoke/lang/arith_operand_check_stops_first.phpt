--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An arithmetic operand php refuses stops it looking at the other one
--DESCRIPTION--
php converts an operator's operands one at a time and STOPS at the first one it refuses, so
the second is never converted and nothing is ever said about it: `$a = "abc"; $b = "5x";
$a + $b;` is the TypeError alone. PHL classified BOTH operands before deciding, so the
leading-numeric right operand announced `A non-numeric value encountered` for a conversion
php never performed — a warning before a fatal, on every arithmetic and integer-only
operator.

The other order is unaffected and is pinned here beside it: a leading-numeric LEFT operand
IS converted, so it warns and then the right operand throws.

Operands are held in variables on purpose — php folds a constant expression at COMPILE time,
which moves its diagnostics out of the error handler installed below.
--FILE--
<?php
set_error_handler(function ($n, $m) { echo '  <', $m, '>', "\n"; return true; });

$aocBad = 'abc';
$aocArr = [1];
$aocLead = '5x';
$aocNum = 5;

foreach (['+', '-', '*', '/', '%', '**', '|', '&', '^', '<<', '>>'] as $aocOp) {
    foreach ([['aocBad', 'aocLead'], ['aocLead', 'aocBad'], ['aocArr', 'aocLead'],
              ['aocLead', 'aocArr'], ['aocLead', 'aocLead'], ['aocLead', 'aocNum'],
              ['aocNum', 'aocLead']] as [$aocL, $aocR]) {
        echo "\$$aocL $aocOp \$$aocR\n";
        try {
            $aocV = eval("return \$GLOBALS['$aocL'] $aocOp \$GLOBALS['$aocR'];");
            echo '  => ', var_export($aocV, true), "\n";
        } catch (\Throwable $e) {
            echo '  [', get_class($e), ': ', $e->getMessage(), "]\n";
        }
    }
}

restore_error_handler();
?>
--EXPECT--
$aocBad + $aocLead
  [TypeError: Unsupported operand types: string + string]
$aocLead + $aocBad
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string + string]
$aocArr + $aocLead
  [TypeError: Unsupported operand types: array + string]
$aocLead + $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string + array]
$aocLead + $aocLead
  <A non-numeric value encountered>
  <A non-numeric value encountered>
  => 10
$aocLead + $aocNum
  <A non-numeric value encountered>
  => 10
$aocNum + $aocLead
  <A non-numeric value encountered>
  => 10
$aocBad - $aocLead
  [TypeError: Unsupported operand types: string - string]
$aocLead - $aocBad
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string - string]
$aocArr - $aocLead
  [TypeError: Unsupported operand types: array - string]
$aocLead - $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string - array]
$aocLead - $aocLead
  <A non-numeric value encountered>
  <A non-numeric value encountered>
  => 0
$aocLead - $aocNum
  <A non-numeric value encountered>
  => 0
$aocNum - $aocLead
  <A non-numeric value encountered>
  => 0
$aocBad * $aocLead
  [TypeError: Unsupported operand types: string * string]
$aocLead * $aocBad
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string * string]
$aocArr * $aocLead
  [TypeError: Unsupported operand types: array * string]
$aocLead * $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string * array]
$aocLead * $aocLead
  <A non-numeric value encountered>
  <A non-numeric value encountered>
  => 25
$aocLead * $aocNum
  <A non-numeric value encountered>
  => 25
$aocNum * $aocLead
  <A non-numeric value encountered>
  => 25
$aocBad / $aocLead
  [TypeError: Unsupported operand types: string / string]
$aocLead / $aocBad
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string / string]
$aocArr / $aocLead
  [TypeError: Unsupported operand types: array / string]
$aocLead / $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string / array]
$aocLead / $aocLead
  <A non-numeric value encountered>
  <A non-numeric value encountered>
  => 1
$aocLead / $aocNum
  <A non-numeric value encountered>
  => 1
$aocNum / $aocLead
  <A non-numeric value encountered>
  => 1
$aocBad % $aocLead
  [TypeError: Unsupported operand types: string % string]
$aocLead % $aocBad
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string % string]
$aocArr % $aocLead
  [TypeError: Unsupported operand types: array % string]
$aocLead % $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string % array]
$aocLead % $aocLead
  <A non-numeric value encountered>
  <A non-numeric value encountered>
  => 0
$aocLead % $aocNum
  <A non-numeric value encountered>
  => 0
$aocNum % $aocLead
  <A non-numeric value encountered>
  => 0
$aocBad ** $aocLead
  [TypeError: Unsupported operand types: string ** string]
$aocLead ** $aocBad
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string ** string]
$aocArr ** $aocLead
  [TypeError: Unsupported operand types: array ** string]
$aocLead ** $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string ** array]
$aocLead ** $aocLead
  <A non-numeric value encountered>
  <A non-numeric value encountered>
  => 3125
$aocLead ** $aocNum
  <A non-numeric value encountered>
  => 3125
$aocNum ** $aocLead
  <A non-numeric value encountered>
  => 3125
$aocBad | $aocLead
  => 'uzc'
$aocLead | $aocBad
  => 'uzc'
$aocArr | $aocLead
  [TypeError: Unsupported operand types: array | string]
$aocLead | $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string | array]
$aocLead | $aocLead
  => '5x'
$aocLead | $aocNum
  <A non-numeric value encountered>
  => 5
$aocNum | $aocLead
  <A non-numeric value encountered>
  => 5
$aocBad & $aocLead
  => '!`'
$aocLead & $aocBad
  => '!`'
$aocArr & $aocLead
  [TypeError: Unsupported operand types: array & string]
$aocLead & $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string & array]
$aocLead & $aocLead
  => '5x'
$aocLead & $aocNum
  <A non-numeric value encountered>
  => 5
$aocNum & $aocLead
  <A non-numeric value encountered>
  => 5
$aocBad ^ $aocLead
  => 'T'
$aocLead ^ $aocBad
  => 'T'
$aocArr ^ $aocLead
  [TypeError: Unsupported operand types: array ^ string]
$aocLead ^ $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string ^ array]
$aocLead ^ $aocLead
  => '' . "\0" . '' . "\0" . ''
$aocLead ^ $aocNum
  <A non-numeric value encountered>
  => 0
$aocNum ^ $aocLead
  <A non-numeric value encountered>
  => 0
$aocBad << $aocLead
  [TypeError: Unsupported operand types: string << string]
$aocLead << $aocBad
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string << string]
$aocArr << $aocLead
  [TypeError: Unsupported operand types: array << string]
$aocLead << $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string << array]
$aocLead << $aocLead
  <A non-numeric value encountered>
  <A non-numeric value encountered>
  => 160
$aocLead << $aocNum
  <A non-numeric value encountered>
  => 160
$aocNum << $aocLead
  <A non-numeric value encountered>
  => 160
$aocBad >> $aocLead
  [TypeError: Unsupported operand types: string >> string]
$aocLead >> $aocBad
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string >> string]
$aocArr >> $aocLead
  [TypeError: Unsupported operand types: array >> string]
$aocLead >> $aocArr
  <A non-numeric value encountered>
  [TypeError: Unsupported operand types: string >> array]
$aocLead >> $aocLead
  <A non-numeric value encountered>
  <A non-numeric value encountered>
  => 0
$aocLead >> $aocNum
  <A non-numeric value encountered>
  => 0
$aocNum >> $aocLead
  <A non-numeric value encountered>
  => 0
