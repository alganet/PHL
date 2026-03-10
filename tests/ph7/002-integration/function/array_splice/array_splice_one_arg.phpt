--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with one argument throws ArgumentCountError
--FILE--
<?php
$a = array(1, 2, 3);
array_splice($a);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_splice() expects at least 2 arguments, 1 given %s
--CLEAN--
<?php
unset($a);
