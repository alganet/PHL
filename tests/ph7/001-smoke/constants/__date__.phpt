--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__DATE__ magic constant expansion
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo "__DATE__ = " . __DATE__ . "\n";
?>
--EXPECTF--
__DATE__ = %s
--CLEAN--
<?php

