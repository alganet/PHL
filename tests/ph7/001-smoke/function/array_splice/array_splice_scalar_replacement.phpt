--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with scalar replacement casts it to array
--FILE--
<?php
$a = array(1, 2, 3);
$r = array_splice($a, 1, 1, 'hello');
echo implode(',', $r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
2
1,hello,3
--CLEAN--
<?php
unset($a, $r);
