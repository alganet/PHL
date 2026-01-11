--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: parent constant returns null when used outside class context
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
if (parent === null) {
    echo "NULL\n";
} else {
    echo "not null\n";
}
?>
--EXPECT--
NULL