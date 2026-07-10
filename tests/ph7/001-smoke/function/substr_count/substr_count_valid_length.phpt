--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_count with valid length parameter
--FILE--
<?php
// Test substr_count with valid length parameter (should not return 0)
$haystack = "abababababa";
$needle = "aba";
$result = substr_count($haystack, $needle, 0, 5); // length 5 is valid
echo "Result: $result\n";
?>
--EXPECT--
Result: 1
--CLEAN--
<?php
unset($haystack, $needle, $result);
