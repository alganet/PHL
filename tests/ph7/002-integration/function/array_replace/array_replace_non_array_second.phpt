--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace with non-array second argument triggers TypeError
--FILE--
<?php
array_replace(array(1), "hello");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_replace(): Argument #2 must be of type array, string given in %s
--CLEAN--
<?php

