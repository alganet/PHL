--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Static variables in functions
--FILE--
<?php
function counter() {
    static $count = 0;
    $count++;
    return $count;
}
echo counter(); // 1
echo "\n";
echo counter(); // 2
echo "\n";
echo counter(); // 3
?>
--EXPECT--
1
2
3
--CLEAN--
<?php
// Function persists, no variables to clean
?>