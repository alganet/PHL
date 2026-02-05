--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object property iteration
--FILE--
<?php
class TestClass {
    public $a = 1;
    public $b = 2;
    public $c = 3;
}

$obj = new TestClass();
$sum = 0;
foreach ($obj as $key => $value) {
    $sum += $value;
    echo "$key: $value\n";
}
echo "Total: $sum\n";
?>
--EXPECT--
a: 1
b: 2
c: 3
Total: 6
--CLEAN--
<?php
unset($obj, $sum);
