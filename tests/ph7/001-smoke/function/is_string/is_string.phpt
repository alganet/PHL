--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_string function
--FILE--
<?php
var_dump(is_string("hello"));
var_dump(is_string(42));
var_dump(is_string(array()));
?>
--EXPECT--
bool(true)
bool(false)
bool(false)
--CLEAN--
<?php

