--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect returns empty array when no common values
--FILE--
<?php
$a = array(1, 2, 3);
$b = array(4, 5, 6);
$c = array_intersect($a, $b);
echo count($c) === 0 ? 'empty' : 'not empty';
?>
--EXPECT--
empty
--CLEAN--
<?php
unset($a, $b, $c);
