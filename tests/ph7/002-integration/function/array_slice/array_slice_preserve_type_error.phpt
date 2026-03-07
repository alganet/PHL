--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing an array as preserve_keys to array_slice triggers TypeError
--FILE--
<?php
array_slice(array(1), 0, 1, array(1));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_slice(): Argument #4 ($preserve_keys) must be of type bool, array given in %s
--CLEAN--
<?php

