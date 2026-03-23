--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert array input throws TypeError
--FILE--
<?php
$a = array(1, 2, 3);
base_convert($a, 10, 16);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: base_convert(): Argument #1 ($num) must be of type string, array given in %s
--CLEAN--
<?php
unset($a);
