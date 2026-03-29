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
%s Fatal error:  Label 'global_label' was referenced but not defined %s
--CLEAN--
<?php

