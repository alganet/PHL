--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
acos with no arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = acos();
echo $result === 0 ? "0" : "not 0";
?>
--EXPECT--
0
--CLEAN--
<?php
unset($result);
