--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr_count with invalid length returns 0
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Length greater than string length
$result1 = substr_count('abc', 'b', 0, 10);
echo "result1=" . $result1 . "\n";
// Negative length
$result2 = substr_count('abc', 'b', 0, -1);
echo "result2=" . $result2 . "\n";
?>
--EXPECT--
result1=0
result2=0