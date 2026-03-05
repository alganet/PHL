--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad with non-array first argument triggers TypeError
--FILE--
<?php
array_pad('not_array', 5, 'x');
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_pad(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

