--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rename() with invalid arguments should return FALSE
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo rename('only_one_arg', array()) ? "true\n" : "false\n";
?>
--EXPECT--
false
--CLEAN--
<?php

