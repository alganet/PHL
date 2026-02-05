--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_replace with array search
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = str_replace(array('a', 'b'), 'x', 'abc');
echo $result === 'xxc' ? 'PASS' : 'FAIL';
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
