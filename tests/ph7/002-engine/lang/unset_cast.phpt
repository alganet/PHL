--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
(unset) type cast
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$value = 42;
$result = (unset) $value;

if ($result === NULL) {
    echo "unset cast to null OK\n";
} else {
    echo "unset cast FAIL\n";
}

$str = "hello";
$result2 = (unset) $str;
if ($result2 === NULL) {
    echo "unset string to null OK\n";
} else {
    echo "unset string FAIL\n";
}

$arr = array(1, 2, 3);
$result3 = (unset) $arr;
if ($result3 === NULL) {
    echo "unset array to null OK\n";
} else {
    echo "unset array FAIL\n";
}
?>
--EXPECT--
unset cast to null OK
unset string to null OK
unset array to null OK