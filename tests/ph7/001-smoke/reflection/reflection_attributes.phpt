--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Attributes: capture, getAttributes, arguments, newInstance
--FILE--
<?php
#[Attribute(Attribute::TARGET_ALL | Attribute::IS_REPEATABLE)]
class ReflAtA {
    public function __construct(public int $x = 0, public string $tag = '') {}
}
#[Attribute(Attribute::TARGET_CLASS)]
class ReflAtClassOnly {}
class ReflAtNot {}

#[ReflAtA(5, tag: 'hello')]
#[ReflAtA]
#[ReflAtA(1 + 2)]
#[ReflAtNot]
#[ReflAtClassOnly]
class ReflAtTarget {
    #[ReflAtA(1)]
    public $prop = 1;
    #[ReflAtA(2)]
    const K = 2;
    #[ReflAtA(3)]
    public function m(#[ReflAtA(4)] $param) {}
}
#[ReflAtA(9)]
function reflAtFn() {}

$rc = new ReflectionClass('ReflAtTarget');
$attrs = $rc->getAttributes();
echo count($attrs), "\n";
foreach ($attrs as $a) { echo $a->getName(), ':', json_encode($a->getArguments()), ':', $a->isRepeated() ? 'rep' : 'once', ':', $a->getTarget(), "\n"; }
$one = $rc->getAttributes('ReflAtA');
echo count($one), "\n";
$inst = $attrs[0]->newInstance();
echo get_class($inst), ':', $inst->x, ':', $inst->tag, "\n";
try { $attrs[3]->newInstance(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $attrs[4]->newInstance(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
echo json_encode((new ReflectionProperty('ReflAtTarget', 'prop'))->getAttributes()[0]->getArguments()), "\n";
echo (new ReflectionProperty('ReflAtTarget', 'prop'))->getAttributes()[0]->getTarget(), "\n";
echo json_encode((new ReflectionClassConstant('ReflAtTarget', 'K'))->getAttributes()[0]->getArguments()), "\n";
$rm = new ReflectionMethod('ReflAtTarget', 'm');
echo json_encode($rm->getAttributes()[0]->getArguments()), ':', $rm->getAttributes()[0]->getTarget(), "\n";
echo json_encode($rm->getParameters()[0]->getAttributes()[0]->getArguments()), ':', $rm->getParameters()[0]->getAttributes()[0]->getTarget(), "\n";
$rf = new ReflectionFunction('reflAtFn');
echo json_encode($rf->getAttributes()[0]->getArguments()), ':', $rf->getAttributes()[0]->getTarget(), "\n";
// filter with IS_INSTANCEOF
echo count($rc->getAttributes('ReflAtNot', ReflectionAttribute::IS_INSTANCEOF)), "\n";
echo ReflectionAttribute::IS_INSTANCEOF, "\n";
// no attrs
class ReflAtBare {}
echo count((new ReflectionClass('ReflAtBare'))->getAttributes()), "\n";
--EXPECT--
5
ReflAtA:{"0":5,"tag":"hello"}:rep:1
ReflAtA:[]:rep:1
ReflAtA:[3]:rep:1
ReflAtNot:[]:once:1
ReflAtClassOnly:[]:once:1
3
ReflAtA:5:hello
Error: Attempting to use non-attribute class "ReflAtNot" as attribute
[1]
8
[2]
[3]:4
[4]:32
[9]:2
1
2
0
