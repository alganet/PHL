--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Store a reference returned from a function into an array index and update values
--FILE--
<?php
function &stor_get_ref(&$x) { return $x; }

$a = 1;
$arr = array();
$arr[0] =& stor_get_ref($a); // store by reference into array index
$arr[0] = 42; // modify through the array reference
echo $a . "\n"; // should reflect the change made via $arr[0]

// Make sure changing original updates the array entry
$a = 100;
echo $arr[0] . "\n";

?>
--EXPECT--
42
100

--CLEAN--
<?php
unset($a, $arr);
?>
