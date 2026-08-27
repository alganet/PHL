--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_numeric function
--FILE--
<?php
var_dump(is_numeric("42"));
var_dump(is_numeric(42));
var_dump(is_numeric(3.14));
var_dump(is_numeric("3.14"));
var_dump(is_numeric("hello"));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
--CLEAN--
<?php

