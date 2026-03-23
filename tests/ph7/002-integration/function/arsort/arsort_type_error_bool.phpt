--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort with bool argument should throw TypeError
--FILE--
<?php
$b = true;
arsort($b);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: arsort(): Argument #1 ($array) must be of type array, %s given in %s
--CLEAN--
<?php
unset($b);
