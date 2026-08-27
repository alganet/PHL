--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_array function
--FILE--
<?php
var_dump(is_array(array()));
var_dump(is_array(42));
?>
--EXPECT--
bool(true)
bool(false)
--CLEAN--
<?php

