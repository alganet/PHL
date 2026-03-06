--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect with empty first array returns empty array
--FILE--
<?php
$a = array();
$b = array(1, 2, 3);
$c = array_intersect($a, $b);
echo count($c) === 0 ? 'empty' : 'not empty';
?>
--EXPECT--
empty
--CLEAN--
<?php
unset($a, $b, $c);
