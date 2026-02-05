--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: E_STRICT constant
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip"; // existing but deprecated in PHP
}
--FILE--
<?php
echo "E_STRICT=" . E_STRICT . "\n";
?>
--EXPECTF--
E_STRICT=%d
--CLEAN--
<?php

