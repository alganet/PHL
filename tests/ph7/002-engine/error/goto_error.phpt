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
%s 3 Error: Label 'global_label' was referenced but not defined
%s 6 Error: Label 'undefined' was referenced but not defined
%s 5 Warning: Label 'global_label' is defined but not referenced
%s 7 Warning: Label 'undefined_label' is defined but not referenced
Compile error
