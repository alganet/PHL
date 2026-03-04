--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_push returns new element count after pushing
--FILE--
<?php
$a = array();
echo array_push($a, 'x', 'y') . PHP_EOL;
?>
--EXPECT--
2
--CLEAN--
<?php
unset($a);
