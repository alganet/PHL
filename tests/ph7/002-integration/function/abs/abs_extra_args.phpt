--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
abs() with too many arguments should raise an ArgumentCountError
--FILE--
<?php
abs(5,7);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: abs() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

