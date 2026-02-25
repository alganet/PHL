--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk called with non-numeric size should throw TypeError
--FILE--
<?php
array_chunk(array(1,2,3), "string");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_chunk(): Argument #2 ($length) must be of type int, string given in %s
--CLEAN--
<?php

