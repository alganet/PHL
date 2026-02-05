--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strcmp basic comparisons
--FILE--
<?php
echo strcmp("abc", "abc") === 0 ? "EQ_OK\n" : "EQ_FAIL\n";
echo strcmp("aaa", "aab") < 0 ? "LT_OK\n" : "LT_FAIL\n";
echo strcmp("aab", "aaa") > 0 ? "GT_OK\n" : "GT_FAIL\n";
?>
--EXPECT--
EQ_OK
LT_OK
GT_OK
--CLEAN--
<?php

