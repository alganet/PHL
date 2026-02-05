--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file_exists() should return FALSE on invalid argument type
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo file_exists(array()) ? "true\n" : "false\n";
?>
--EXPECT--
false
--CLEAN--
<?php

