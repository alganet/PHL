--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Ellipsis token does not break dot concatenation or .= operator
--FILE--
<?php
// Dot concatenation still works
echo "hello" . " " . "world" . "\n";

// .= still works
$s = "hello";
$s .= " world";
echo $s . "\n";

// Ellipsis in variadic declaration
function collect(...$items) {
    return count($items);
}
echo collect(1, 2, 3) . "\n";
?>
--EXPECT--
hello world
hello world
3
--CLEAN--
<?php

