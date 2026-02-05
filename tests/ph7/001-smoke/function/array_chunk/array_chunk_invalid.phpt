--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// No arguments
$result = array_chunk();
if ($result === null || $result === false) echo "PASS\n"; else echo "FAIL\n";
// Non-array argument
$result = array_chunk("not array", 2);
if ($result === null || $result === false) echo "PASS\n"; else echo "FAIL\n";
// Invalid chunk size type
$result = array_chunk(array(1,2,3), "string");
if ($result === null || $result === false) echo "PASS\n"; else echo "FAIL\n";
?>
--EXPECT--
PASS
PASS
PASS
--CLEAN--
<?php
unset($result);
