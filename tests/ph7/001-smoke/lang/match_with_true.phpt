--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: match(true) idiom with boolean predicates as conditions
--FILE--
<?php
$classify = function ($n) {
    return match (true) {
        $n < 0  => 'negative',
        $n === 0 => 'zero',
        $n < 10 => 'small',
        $n < 100 => 'medium',
        default  => 'big',
    };
};
echo $classify(-5), "\n";
echo $classify(0), "\n";
echo $classify(7), "\n";
echo $classify(42), "\n";
echo $classify(999), "\n";
?>
--EXPECT--
negative
zero
small
medium
big
--CLEAN--
<?php
unset($classify);
