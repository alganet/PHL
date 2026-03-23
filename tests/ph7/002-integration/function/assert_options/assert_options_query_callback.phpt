--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert_options(ASSERT_CALLBACK) returns null by default
--FILE--
<?php
error_reporting(E_ALL & ~E_DEPRECATED);
echo assert_options(ASSERT_CALLBACK) === null ? "null" : "not_null";
?>
--EXPECT--
null
--CLEAN--
<?php

