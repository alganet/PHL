--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_push with a single value returns 1
--FILE--
<?php
$a = array();
echo array_push($a, 'hello') . PHP_EOL;
?>
--EXPECT--
1
--CLEAN--
<?php
unset($a);
