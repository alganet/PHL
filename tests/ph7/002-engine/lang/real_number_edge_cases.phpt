--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Real number literal edge cases
--FILE--
<?php
// Test various real number literal formats
echo 3.14 . "\n";
echo 1.0 . "\n";
?>
--EXPECT--
3.14
1
--CLEAN--
<?php
?>