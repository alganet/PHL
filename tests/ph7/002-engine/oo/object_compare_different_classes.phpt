--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object comparison between different classes
--FILE--
<?php
class ClassA {
    public $value = 1;
}

class ClassB {
    public $value = 1;
}

$a = new ClassA();
$b = new ClassB();

// Different classes should not be equal with ==
$not_equal = ($a == $b);
echo $not_equal ? "not_equal_fail" : "not_equal_ok";
echo "\n";

// Different classes should not be identical with ===
$not_identical = ($a === $b);
echo $not_identical ? "not_identical_fail" : "not_identical_ok";
echo "\n";
?>
--EXPECT--
not_equal_ok
not_identical_ok