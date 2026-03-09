--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map with undefined function name throws TypeError
--FILE--
<?php
array_map("nonexistent_func", array(1, 2, 3));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, function "nonexistent_func" not found or invalid function name in %s
--CLEAN--
<?php

