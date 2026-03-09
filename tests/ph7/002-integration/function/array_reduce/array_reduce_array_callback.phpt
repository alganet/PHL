--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce with array callback throws TypeError
--FILE--
<?php
array_reduce(array(1, 2), array(1));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_reduce(): Argument #2 ($callback) must be a valid callback, array callback must have exactly two members in %s
--CLEAN--
<?php

