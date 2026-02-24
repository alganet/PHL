--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object comparison of same instance with == operator
--FILE--
<?php
class ObjectCompareSameInstanceClass {
    public $value = 10;
}

$obj = new ObjectCompareSameInstanceClass();

// Compare object with itself using ==
$self_equal = ($obj == $obj);
echo $self_equal ? "self_equal_ok" : "self_equal_fail";
echo "\n";

// Compare object with itself using ===
$self_identical = ($obj === $obj);
echo $self_identical ? "self_identical_ok" : "self_identical_fail";
echo "\n";
?>
--EXPECT--
self_equal_ok
self_identical_ok
--CLEAN--
<?php
unset($obj, $self_equal, $self_identical);
