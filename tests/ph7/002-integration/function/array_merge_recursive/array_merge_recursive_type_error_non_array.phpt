--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge_recursive() should throw a TypeError when given a non-array argument
--FILE--
<?php
array_merge_recursive(array(1), "x");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_merge_recursive(): Argument #2 must be of type array, string given in %s
--CLEAN--
<?php

