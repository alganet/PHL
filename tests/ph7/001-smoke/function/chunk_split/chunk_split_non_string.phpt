--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chunk_split with non-string argument
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = chunk_split(123);
echo $result === null ? 'PASS' : 'FAIL';
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
