--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice replacing all elements with replacement
--FILE--
<?php
$a = array(1, 2, 3);
$r = array_splice($a, 0, 3, array('a', 'b'));
echo implode(',', $r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
1,2,3
a,b
--CLEAN--
<?php
unset($a, $r);
