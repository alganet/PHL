--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chunk_split with chunklen larger than string length
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = chunk_split("hello", 10);
echo $result === "hello\r\n" ? "PASS" : "FAIL";
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
