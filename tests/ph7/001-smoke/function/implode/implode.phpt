--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode basic functionality
--FILE--
<?php
// Basic implode with separator
$result1 = implode(",", array("a", "b", "c"));
echo $result1 . "\n";

// Basic implode without separator
$result2 = implode(array("x", "y", "z"));
echo $result2 . "\n";

// Implode with empty separator
$result3 = implode("", array("foo", "bar"));
echo $result3 . "\n";

// Implode with numeric values
$result4 = implode("-", array(1, 2, 3));
echo $result4 . "\n";
?>
--EXPECT--
a,b,c
xyz
foobar
1-2-3
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4);
