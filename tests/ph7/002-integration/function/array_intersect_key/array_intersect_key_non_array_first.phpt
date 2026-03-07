--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key with non-array first argument triggers TypeError
--FILE--
<?php
array_intersect_key('hello', array(1, 2));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_intersect_key(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

