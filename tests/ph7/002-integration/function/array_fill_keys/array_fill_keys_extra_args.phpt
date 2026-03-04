--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys: too many arguments triggers ArgumentCountError
--FILE--
<?php
$a = array('x');
array_fill_keys($a, 1, 2);
?>
--EXPECTF--
PHP Fatal error:  Uncaught ArgumentCountError: array_fill_keys() expects exactly 2 arguments, 3 given in %s
--CLEAN--
<?php
unset($a);
