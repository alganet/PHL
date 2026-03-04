--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: declare statement with invalid syntax (missing opening parenthesis)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
declare ticks=1) {
    echo "This should not execute\n";
}
?>
--EXPECTF--
%s 2 Error:  declare: Expecting opening parenthesis '('
Compile error
--CLEAN--
<?php

