--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode with large array memory stress test
--FILE--
<?php
// Test implode with large array that may trigger memory allocation edge cases
$large_array = array();
for ($i = 0; $i < 10000; $i++) {
    $large_array[] = str_repeat('x', 10);
}
$result = implode(',', $large_array);
$length = strlen($result);
echo "Imploded string length: $length\n";
?>
--EXPECT--
Imploded string length: 109999