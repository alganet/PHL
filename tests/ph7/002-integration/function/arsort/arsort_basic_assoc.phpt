--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort basic associative array reverse sort with key preservation
--FILE--
<?php
$a = array('d' => 'lemon', 'a' => 'orange', 'b' => 'banana', 'c' => 'apple');
arsort($a);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
a: orange
d: lemon
b: banana
c: apple
--CLEAN--
<?php
unset($a);
