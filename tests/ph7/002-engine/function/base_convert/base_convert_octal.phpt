--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert: Octal conversion
--FILE--
<?php
$result = base_convert('7', 8, 10);
echo "Result: $result\n";
?>
--EXPECT--
Result: 7