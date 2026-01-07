--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
List assignment with various edge cases
--FILE--
<?php
// Test basic list assignment
list($a, $b, $c) = array(1, 2, 3);
echo "Basic: a=$a, b=$b, c=$c\n";

// Test list with fewer elements than array
list($x, $y) = array(10, 20, 30);
echo "Fewer elements: x=$x, y=$y\n";

// Test list with skipped elements
list($first, , $third) = array('alpha', 'beta', 'gamma');
echo "Skipped: first=$first, third=$third\n";

// Test multiple assignments
list($p, $q) = array(100, 200);
list($r, $s) = array(300, 400);
echo "Multiple: p=$p, q=$q, r=$r, s=$s\n";
?>
--EXPECT--
Basic: a=1, b=2, c=3
Fewer elements: x=10, y=20
Skipped: first=alpha, third=gamma
Multiple: p=100, q=200, r=300, s=400