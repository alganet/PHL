--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP: soundex empty string
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo 'skip';
}
?>
--FILE--
<?php
// Test empty string
echo soundex("");
?>
--EXPECT--
0000
--CLEAN--
<?php

