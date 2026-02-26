--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_pop with too many arguments triggers ArgumentCountError
--FILE--
<?php
$a = array(1,2);
array_pop($a, 123);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_pop() expects exactly 1 argument, %d given in %s
