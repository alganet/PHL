--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge with non-array first argument triggers TypeError
--FILE--
<?php
array_merge(1, 2);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_merge(): Argument #1 must be of type array, int given in %s
--CLEAN--
<?php

