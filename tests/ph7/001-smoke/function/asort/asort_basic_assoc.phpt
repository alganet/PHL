--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort basic associative array sort with key preservation
--FILE--
<?php
$a = array('d' => 'lemon', 'a' => 'orange', 'b' => 'banana', 'c' => 'apple');
asort($a);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
c: apple
b: banana
d: lemon
a: orange
--CLEAN--
<?php
unset($a);
