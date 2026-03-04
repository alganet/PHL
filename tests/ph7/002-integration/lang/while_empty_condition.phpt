--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
While with empty condition
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
while () {
    echo "loop\n";
}
?>
--EXPECTF--
%s 2 Error:  Expected expression after 'while' keyword
Compile error
--CLEAN--
<?php

