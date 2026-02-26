--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_shift with too many arguments triggers ArgumentCountError
--FILE--
<?php
$a = array(1,2);
array_shift($a, 123);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_shift() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php
unset($a);
