--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk with missing arguments should throw ArgumentCountError
--FILE--
<?php
array_chunk();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_chunk() expects at least 2 arguments, %d given in %s
--CLEAN--
<?php

