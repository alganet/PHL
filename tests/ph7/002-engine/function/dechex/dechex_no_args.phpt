--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
dechex with no arguments
--FILE--
<?php
$result = dechex();
if ($result === null) {
    echo "NULL\n";
} else {
    echo "NOT NULL\n";
}
?>
--EXPECT--
NULL