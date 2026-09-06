--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: goto errors for undefined and unreachable labels
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function f() {
    goto global_label;
}
global_label:
goto undefined;
undefined_label:
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'global_label'%AFatal error:%A'goto' to undefined label 'undefined'%A
--CLEAN--
<?php

