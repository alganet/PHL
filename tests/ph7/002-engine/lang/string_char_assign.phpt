--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Assign character into a string index (string articulation branch for STORE_IDX)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$s = "abc";
$s[1] = 'X';
echo $s . "\n";
// Also test append to string using empty key assignment
$s2 = "A";
$s2[] = 'Z';
echo $s2 . "\n";
?>
--EXPECT--
aXc
AZ

--CLEAN--
<?php
unset($s, $s2);
?>
