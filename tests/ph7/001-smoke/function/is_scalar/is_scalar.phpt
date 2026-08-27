--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_scalar function
--FILE--
<?php
var_dump(is_scalar(42));
var_dump(is_scalar("hello"));
var_dump(is_scalar(3.14));
var_dump(is_scalar(true));
var_dump(is_scalar(array()));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
--CLEAN--
<?php

