--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map with non-callable first argument throws TypeError
--FILE--
<?php
array_map(42, array(1, 2, 3));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, no array or string given in %s
--CLEAN--
<?php

