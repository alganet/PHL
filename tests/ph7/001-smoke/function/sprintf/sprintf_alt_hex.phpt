--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: sprintf supports alternate form for hex (%#x)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$ret = sprintf('%#x', 255);
echo "alt_hex=" . $ret . "\n";
?>
--EXPECT--
alt_hex=0xff
--CLEAN--
<?php
unset($ret);
