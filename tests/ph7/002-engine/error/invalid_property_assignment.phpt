--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
undefined object property assignment error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$obj = new stdClass();
$obj->nonexistent = "value";
?>
--EXPECTF--
%s Notice: Missing constructor argument 1($v) for class 'stdClass'
%s Error: Undefined class attribute 'stdClass->nonexistent',PH7 is loading NULL
%s Error: Cannot perform assignment on a constant class attribute,PH7 is loading NULL