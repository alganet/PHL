--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing an array literal should trigger the same reference error
--FILE--
<?php
array_push(array(), 'value');
?>
--EXPECTF--
%s Fatal error:  Uncaught Error: array_push(): Argument #1 ($array) could not be passed by reference in %s
--CLEAN--
<?php

