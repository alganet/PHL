--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_push onto a non-empty array adds to the end
--FILE--
<?php
$a = array('a', 'b');
echo array_push($a, 'c') . PHP_EOL;
?>
--EXPECT--
3
--CLEAN--
<?php
unset($a);
