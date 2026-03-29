--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto label in different function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function foo() {
goto label;
}
label:
echo "hello";
--EXPECTF--
%s Fatal error:  Label 'label' was referenced but not defined %s
--CLEAN--
<?php

