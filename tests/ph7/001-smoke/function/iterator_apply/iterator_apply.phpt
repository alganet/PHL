--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
iterator_apply invokes the callback per element, stops on false, returns the count
--FILE--
<?php
function iaGen() { yield 1; yield 2; yield 3; yield 4; }
// Runs for every element (callback returns true) -> full count.
echo iterator_apply(iaGen(), function () { return true; }), "\n";
// Stops once the callback returns false (after the 3rd call).
echo iterator_apply(iaGen(), function () { static $n = 0; $n++; return $n < 3; }), "\n";
// The fixed $args are passed to the callback each iteration.
echo iterator_apply(iaGen(), function ($step) { return $step === 10; }, [10]), "\n";
?>
--EXPECT--
4
3
4
--CLEAN--
<?php
