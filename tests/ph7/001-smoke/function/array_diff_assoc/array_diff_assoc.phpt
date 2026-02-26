--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_assoc compares both key and value
--FILE--
<?php
$d = array_diff_assoc(array('a' => 1, 'b' => 2), array('a' => 1));
echo implode(',', array_keys($d)) . PHP_EOL; // expecting 'b'
?>
--EXPECT--
b
--CLEAN--
<?php
unset($d);
