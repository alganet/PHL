--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
array_filter called without arguments should return NULL (PHL-specific)
--FILE--
<?php
$result = array_filter();
if ($result === null) echo "NULL\n"; else echo "NOT NULL\n";
?>
--EXPECT--
NULL
--CLEAN--
<?php
unset($result);
