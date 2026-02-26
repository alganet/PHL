--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_combine with non-array values should throw TypeError
--FILE--
<?php
array_combine(array(), 1);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_combine(): Argument #2 ($values) must be of type array, %s given in %s
--CLEAN--
<?php

