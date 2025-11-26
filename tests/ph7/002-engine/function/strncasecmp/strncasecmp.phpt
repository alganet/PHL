--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strncasecmp case-insensitive n-char comparisons
--FILE--
<?php
echo strncasecmp("HelloWorld", "helloYOU", 5) === 0 ? "CI_EQ_N_OK\n" : "CI_EQ_N_FAIL\n";
echo strncasecmp("abcxx", "abdzz", 2) === 0 ? "CI_EQ_N2_OK\n" : "CI_EQ_N2_FAIL\n";
?>
--EXPECT--
CI_EQ_N_OK
CI_EQ_N2_OK
