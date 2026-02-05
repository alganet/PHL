--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
function with invalid argument type
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function foo(echo $a) {
}
--EXPECTF--
%s 2 Warning: Invalid argument type 'echo',Automatic cast will not be performed
--CLEAN--
<?php

