--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strcasecmp with longer strings
--FILE--
<?php
// Test with longer strings to cover more loop iterations
echo strcasecmp("abcdefghij", "ABCDEFGHIJ") === 0 ? "LONG_CI_EQ_OK\n" : "LONG_CI_EQ_FAIL\n";
echo strcasecmp("abcdefghijklmn", "ABCDEFGHIJKLNO") < 0 ? "LONG_CI_LT_OK\n" : "LONG_CI_LT_FAIL\n";
?>
--EXPECT--
LONG_CI_EQ_OK
LONG_CI_LT_OK