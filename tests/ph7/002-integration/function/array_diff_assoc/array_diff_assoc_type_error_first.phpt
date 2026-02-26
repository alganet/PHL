--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing a non-array first argument should raise a TypeError
--FILE--
<?php
array_diff_assoc('not an array');
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff_assoc(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

