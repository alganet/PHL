--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
floor() should typecheck its argument and reject non‑numeric strings
--FILE--
<?php
floor("foo");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: floor(): Argument #1 ($num) must be of type int|float, string given in %s
--CLEAN--
<?php

?>
