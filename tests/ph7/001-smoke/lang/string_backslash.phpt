--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
single quoted string with backslash followed by non-special character
--FILE--
<?php
$a = 'hello\world';
echo $a . "\n";
?>
--EXPECT--
hello\world
--CLEAN--
<?php
unset($a);
