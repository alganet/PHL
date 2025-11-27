--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ISO_8859_1 constant
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
echo "ISO_8859_1=" . ISO_8859_1 . "\n";
?>
--EXPECTF--
ISO_8859_1=%s
