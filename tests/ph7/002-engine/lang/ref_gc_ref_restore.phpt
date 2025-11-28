--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Reference and unset with array copy
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$arr1 = array(1, 2, 3);
$arr2 = &$arr1;
unset($arr1);
if (is_array($arr2)) {
  echo "kept\n";
} else {
  echo "lost\n";
}

$a = array(4, 5);
$b = &$a;
$b[0] = 999;
echo implode(',', $a) . "\n";
?>
--EXPECT--
lost
999,5

--CLEAN--
<?php
unset($a, $b, $arr2);
?>
