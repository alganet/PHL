--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_combine with non-array keys should throw TypeError
--FILE--
<?php
array_combine(1, array());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_combine(): Argument #1 ($keys) must be of type array, %s given in %s
--CLEAN--
<?php

