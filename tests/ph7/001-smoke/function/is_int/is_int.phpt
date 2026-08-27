--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_int function
--FILE--
<?php
var_dump(is_int(42));
var_dump(is_int(3.14));
var_dump(is_int("42"));
?>
--EXPECT--
bool(true)
bool(false)
bool(false)
--CLEAN--
<?php

