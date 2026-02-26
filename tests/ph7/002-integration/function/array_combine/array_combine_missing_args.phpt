--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_combine with missing arguments should throw ArgumentCountError
--FILE--
<?php
array_combine();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_combine() expects exactly 2 arguments, %d given in %s
--CLEAN--
<?php

