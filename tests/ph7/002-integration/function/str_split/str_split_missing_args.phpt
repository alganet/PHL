--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling str_split with no arguments triggers ArgumentCountError
--FILE--
<?php
str_split();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: str_split() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

