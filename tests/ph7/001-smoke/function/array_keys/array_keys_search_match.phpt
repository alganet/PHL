--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys returns keys matching a search value with loose comparison
--FILE--
<?php
$a = array('a' => 10, 'b' => 20, 'c' => 10, 'd' => 30);
$k = array_keys($a, 10);
echo implode(',', $k);
?>
--EXPECT--
a,c
--CLEAN--
<?php
unset($a, $k);
