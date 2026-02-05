--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Do without while
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
do {
    echo "loop\n";
}
?>
--EXPECTF--
%s 2 Error: Missing 'while' statement after 'do' block
Compile error
--CLEAN--
<?php

