--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_float function
--FILE--
<?php
var_dump(is_float(3.14));
var_dump(is_float(42));
var_dump(is_float("3.14"));
?>
--EXPECT--
bool(true)
bool(false)
bool(false)
--CLEAN--
<?php

