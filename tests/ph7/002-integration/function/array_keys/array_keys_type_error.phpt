--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_keys with non-array argument triggers TypeError
--FILE--
<?php
array_keys('not_an_array');
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_keys(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

