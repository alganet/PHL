--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_flip with no arguments triggers ArgumentCountError
--FILE--
<?php
$array = array(1,2,3);
array_flip();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_flip() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php
unset($array);
