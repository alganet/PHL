--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
settype() with an unknown type name throws ValueError
--FILE--
<?php
$x = 1;
settype($x, "bogus");
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: settype(): Argument #2 ($type) must be a valid type in %s
