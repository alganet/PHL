--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strcasecmp case-insensitive comparisons
--FILE--
<?php
echo strcasecmp("Hello", "hello") === 0 ? "CI_EQ_OK\n" : "CI_EQ_FAIL\n";
echo strcasecmp("abc", "ABD") < 0 ? "CI_LT_OK\n" : "CI_LT_FAIL\n";
echo strcasecmp("ABD", "abc") > 0 ? "CI_GT_OK\n" : "CI_GT_FAIL\n";
?>
--EXPECT--
CI_EQ_OK
CI_LT_OK
CI_GT_OK
--CLEAN--
<?php

