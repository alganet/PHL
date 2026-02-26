--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Non-array first argument should raise TypeError
--FILE--
<?php
array_diff('foo', array());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

