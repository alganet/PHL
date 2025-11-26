--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strncmp basic comparisons
--FILE--
<?php
echo strncmp("abcdef", "abcxyz", 3) === 0 ? "EQ3_OK\n" : "EQ3_FAIL\n";
echo strncmp("abc", "abd", 2) === 0 ? "EQ2_OK\n" : "EQ2_FAIL\n";
echo strncmp("abc", "ab", 3) > 0 ? "GT_OK\n" : "GT_FAIL\n";
?>
--EXPECT--
EQ3_OK
EQ2_OK
GT_OK
