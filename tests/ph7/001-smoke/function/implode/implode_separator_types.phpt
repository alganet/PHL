--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode with different separator types
--FILE--
<?php
// Test implode with numeric separator
$result1 = implode(123, array("a", "b"));
echo $result1 === "a123b" ? "NUMERIC_SEP_OK\n" : "NUMERIC_SEP_FAIL: $result1\n";

// Test implode with float separator
$result4 = implode(1.5, array("m", "n"));
echo $result4 === "m1.5n" ? "FLOAT_SEP_OK\n" : "FLOAT_SEP_FAIL: $result4\n";
?>
--EXPECT--
NUMERIC_SEP_OK
FLOAT_SEP_OK
--CLEAN--
<?php
unset($result1, $result4);
