--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionParameter: names, positions, defaults, byref, variadic
--FILE--
<?php
const REFL_PAR_C = 'cval';
function reflParFn($first, &$ref, $def = REFL_PAR_C, $arr = array(1, 2), ...$tail) {}
class ReflParCls {
    public function m($x, $y = 3) { return $x + $y; }
}

$rf = new ReflectionFunction('reflParFn');
$ps = $rf->getParameters();
echo count($ps), "\n";
foreach ($ps as $p) {
    echo $p->getName(), ':', $p->getPosition(),
        ($p->isPassedByReference() ? ':ref' : ':val'),
        ($p->isVariadic() ? ':var' : ':fix'),
        ($p->isOptional() ? ':opt' : ':req'),
        ($p->isDefaultValueAvailable() ? ':def' : ':nodef'), "\n";
}
echo $ps[2]->getDefaultValue(), "\n";
echo $ps[2]->isDefaultValueConstant() ? 'const:' . $ps[2]->getDefaultValueConstantName() : 'not-const', "\n";
echo json_encode($ps[3]->getDefaultValue()), "\n";
echo $ps[3]->isDefaultValueConstant() ? 'const' : 'not-const', "\n";
try {
    $ps[0]->getDefaultValue();
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
echo $ps[0]->getDeclaringFunction()->name, "\n";
echo $ps[0]->getDeclaringClass() === null ? 'no-class' : 'class', "\n";
echo $ps[0]->canBePassedByValue() ? 'canval' : 'noval', "\n";

$pn = new ReflectionParameter('reflParFn', 'def');
echo $pn->getPosition(), "\n";
$pi = new ReflectionParameter('reflParFn', 1);
echo $pi->getName(), "\n";
try {
    new ReflectionParameter('reflParFn', 99);
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    new ReflectionParameter('reflParFn', 'nope');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

$pm = new ReflectionParameter(array('ReflParCls', 'm'), 1);
echo $pm->getName(), ':', $pm->getDefaultValue(), "\n";
echo $pm->getDeclaringClass()->name, "\n";
echo $pm->getDeclaringFunction()->name, "\n";

$rm = new ReflectionMethod('ReflParCls', 'm');
$mp = $rm->getParameters();
echo count($mp), ':', $mp[0]->getName(), "\n";
?>
--EXPECT--
5
first:0:val:fix:req:nodef
ref:1:ref:fix:req:nodef
def:2:val:fix:opt:def
arr:3:val:fix:opt:def
tail:4:val:var:opt:nodef
cval
const:REFL_PAR_C
[1,2]
not-const
ReflectionException: Internal error: Failed to retrieve the default value
reflParFn
no-class
canval
2
ref
ReflectionException: The parameter specified by its offset could not be found
ReflectionException: The parameter specified by its name could not be found
y:3
ReflParCls
m
2:x
