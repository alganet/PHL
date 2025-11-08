--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in functions
--FILE--
<?php
echo strlen("hello"); // 5
echo "\n";
$a = array(1,2,3);
echo count($a); // 3
?>
--EXPECT--
5
3
--CLEAN--
<?php
unset($a);
?>