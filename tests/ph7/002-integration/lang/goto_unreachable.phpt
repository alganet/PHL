--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Goto unreachable label
--FILE--
<?php
bar:
function foo() {
    goto bar;
}
echo "ok";
?>
--EXPECTF--
%s %d Error: Label 'bar' is unreachable
Compile error
--CLEAN--
<?php

