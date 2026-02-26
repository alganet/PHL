--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A non-array third argument should trigger a TypeError
--FILE--
<?php
array_diff_assoc(array(), array(), 123);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff_assoc(): Argument #3 must be of type array, int given in %s
--CLEAN--
<?php

