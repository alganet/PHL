--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: soundex empty string
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo 'skip';
}
?>
--FILE--
<?php
// Test empty string
echo soundex("") === "?000" ? "EMPTY_OK\n" : "EMPTY_FAIL: " . soundex("") . "\n";
?>
--EXPECT--
EMPTY_OK
--CLEAN--
<?php

