--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys: integer first argument triggers TypeError
--FILE--
<?php
array_fill_keys(42, 'val');
?>
--EXPECTF--
PHP Fatal error:  Uncaught TypeError: array_fill_keys(): Argument #1 ($keys) must be of type array, int given in %s
--CLEAN--
<?php

