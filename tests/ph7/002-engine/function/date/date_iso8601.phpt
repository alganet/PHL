--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: date ISO 8601 format
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Test ISO 8601 format
$result = date("c");
echo strlen($result) === 24 ? "ISO8601_OK\n" : "ISO8601_FAIL: '$result'\n";
?>
--EXPECT--
ISO8601_OK
