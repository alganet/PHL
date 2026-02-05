--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert: Hexadecimal conversion
--FILE--
<?php
$result = base_convert('a', 16, 10);
echo "Result: $result\n";
?>
--EXPECT--
Result: 10
--CLEAN--
<?php
unset($result);
