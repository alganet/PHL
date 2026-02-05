--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: __PH7__ string value
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo "__PH7__=" . __PH7__ . "\n";
?>
--EXPECTF--
__PH7__=PH7/%s
--CLEAN--
<?php

