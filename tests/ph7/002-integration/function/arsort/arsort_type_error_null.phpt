--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort with null argument should throw TypeError
--FILE--
<?php
$n = null;
arsort($n);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: arsort(): Argument #1 ($array) must be of type array, null given in %s
--CLEAN--
<?php
unset($n);
