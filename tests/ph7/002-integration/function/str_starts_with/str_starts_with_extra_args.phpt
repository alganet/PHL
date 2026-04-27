--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_starts_with() with too many arguments raises ArgumentCountError
--FILE--
<?php
str_starts_with("a", "b", "c");
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: str_starts_with() expects exactly 2 arguments, 3 given in %s
--CLEAN--
<?php

