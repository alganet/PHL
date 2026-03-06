--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with negative split length triggers ValueError
--FILE--
<?php
str_split("hello", -1);
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: str_split(): Argument #2 ($length) must be greater than 0 in %s
--CLEAN--
<?php

