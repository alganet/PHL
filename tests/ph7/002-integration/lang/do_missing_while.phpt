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
%s Error:  Missing 'while' statement after 'do' block %s
--CLEAN--
<?php

