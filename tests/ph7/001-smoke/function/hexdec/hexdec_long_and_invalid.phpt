--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec with long hex and invalid character behavior
--SKIPIF--
<?php if (function_exists('zend_version')) { echo "skip: not PH7\n"; } ?>
--FILE--
<?php
echo "long:" . hexdec('0123456789abcdef0') . "\n";
echo "invalid:" . hexdec('DEADBEEFZ') . "\n";
?>
--EXPECT--
long:1311768467463790320
invalid:3735928559
--CLEAN--
<?php

