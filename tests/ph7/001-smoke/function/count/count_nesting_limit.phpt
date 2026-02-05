--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE on deeply nested array to trigger nesting limit
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Create a deeply nested array with more than 31 levels
$a = array();
for ($i = 0; $i < 35; $i++) {
    $a = array($a);
}
echo @count($a, COUNT_RECURSIVE) . "\n";
?>
--EXPECT--
32
--CLEAN--
<?php
unset($a);
