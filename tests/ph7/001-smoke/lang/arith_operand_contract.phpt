--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arithmetic operand contract: numeric strings, warnings and TypeErrors
--FILE--
<?php
function aocTry($fn) {
    try {
        var_export($fn());
        echo "\n";
    } catch (TypeError $e) {
        echo "TypeError: ", $e->getMessage(), "\n";
    }
}
set_error_handler(function ($no, $msg) { echo "Warning: $msg\n"; return true; });

// Fully numeric strings are plain numbers.
aocTry(fn() => "1e3" + 1);
aocTry(fn() => " 5 " * 2);
aocTry(fn() => ".5" + 0);

// A leading-numeric string warns and computes with the prefix. A string is
// always base 10, so "0x1A" is the prefix "0" with a tail.
aocTry(fn() => "5abc" + 10);
aocTry(fn() => 10 - "3x");
aocTry(fn() => "0x1A" + 0);
aocTry(fn() => "1e" + 0);

// No numeric prefix at all is a TypeError, naming both operand types.
aocTry(fn() => 5 + "abc");
aocTry(fn() => 1 + "");
aocTry(fn() => "abc" ** 2);
aocTry(fn() => 7 % "2z");
aocTry(fn() => [1] + 1);
aocTry(fn() => 1 + new stdClass());

// array + array stays php's union operator.
aocTry(fn() => [1, 2] + [3, 4, 5]);

// Compound assignment enforces the same contract.
aocTry(function () { $v = 5; $v -= "xyz"; return $v; });
aocTry(function () { $v = 5; $v /= "2"; return $v; });

// int/int division is an int when exact, a float otherwise.
aocTry(fn() => 6 / 3);
aocTry(fn() => 7 / 2);
aocTry(fn() => "10" - "4");
restore_error_handler();
?>
--EXPECT--
1001.0
10
0.5
Warning: A non-numeric value encountered
15
Warning: A non-numeric value encountered
7
Warning: A non-numeric value encountered
0
Warning: A non-numeric value encountered
1
TypeError: Unsupported operand types: int + string
TypeError: Unsupported operand types: int + string
TypeError: Unsupported operand types: string ** int
Warning: A non-numeric value encountered
1
TypeError: Unsupported operand types: array + int
TypeError: Unsupported operand types: int + stdClass
array (
  0 => 1,
  1 => 2,
  2 => 5,
)
TypeError: Unsupported operand types: int - string
2.5
2
3.5
6
--CLEAN--
<?php
