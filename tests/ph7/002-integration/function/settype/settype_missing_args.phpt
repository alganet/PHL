--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
settype() with too few arguments throws ArgumentCountError
--FILE--
<?php
$x = 1;
settype($x);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: settype() expects exactly 2 arguments, 1 given in %s
