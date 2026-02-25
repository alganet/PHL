--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk with non-array first argument should throw TypeError
--FILE--
<?php
array_chunk("not array",2);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_chunk(): Argument #1 ($array) must be of type array, %s given in %s
--CLEAN--
<?php

