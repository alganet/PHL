--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with string argument should throw TypeError
--FILE--
<?php
$s = "hello";
asort($s);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: asort(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php
unset($s);
