--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
floatval conversions edge cases
--FILE--
<?php
echo (float)'1.23e2' . "\n";
echo (float)'.5' . "\n";
echo (float)'1.' . "\n";
echo (float)'invalid' . "\n";
?>
--EXPECT--
123
0.5
1
0
