--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass instantiation: newInstance, newInstanceArgs, newInstanceWithoutConstructor
--FILE--
<?php
class ReflInstPoint {
    public $x;
    public $y;
    public function __construct($x, $y = 100) {
        $this->x = $x;
        $this->y = $y;
    }
}
class ReflInstNoCtor {
    public $set = 'default';
}
abstract class ReflInstAbstract {}

$rc = new ReflectionClass('ReflInstPoint');
$p = $rc->newInstance(1, 2);
echo $p->x, ',', $p->y, "\n";
$p2 = $rc->newInstance(5);
echo $p2->x, ',', $p2->y, "\n";
$p3 = $rc->newInstanceArgs(array(7, 8));
echo $p3->x, ',', $p3->y, "\n";
$p4 = $rc->newInstanceWithoutConstructor();
echo $p4->x === null ? 'null' : 'set', "\n";
echo get_class($p4), "\n";

$rn = new ReflectionClass('ReflInstNoCtor');
$n = $rn->newInstance();
echo $n->set, "\n";
try {
    $rn->newInstance('arg');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

$ra = new ReflectionClass('ReflInstAbstract');
try {
    $ra->newInstance();
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    $ra->newInstanceWithoutConstructor();
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
1,2
5,100
7,8
null
ReflInstPoint
default
ReflectionException: Class ReflInstNoCtor does not have a constructor, so you cannot pass any constructor arguments
Error: Cannot instantiate abstract class ReflInstAbstract
Error: Cannot instantiate abstract class ReflInstAbstract
