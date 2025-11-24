--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_repeat repeats a string n times
--FILE--
<?php
echo str_repeat('a', 3) . "\n";
?>
--EXPECT--
aaa
