--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reflection over internal functions via the signature table
--FILE--
<?php
$r = new ReflectionFunction('strlen');
echo $r->getNumberOfParameters(), ',', $r->getNumberOfRequiredParameters(), "\n";
$p = $r->getParameters();
echo $p[0]->getName(), ':', (string)$p[0]->getType(), "\n";
$r2 = new ReflectionFunction('substr');
echo $r2->getNumberOfParameters(), ',', $r2->getNumberOfRequiredParameters(), "\n";
$ps = $r2->getParameters();
foreach ($ps as $q) {
    echo $q->getName(), ':', $q->isOptional() ? 'opt' : 'req', ':', (string)$q->getType(), "\n";
}
echo json_encode($ps[2]->getDefaultValue()), "\n";
$r3 = new ReflectionFunction('str_replace');
$p3 = $r3->getParameters();
echo $p3[3]->getName(), ':', $p3[3]->isPassedByReference() ? 'ref' : 'val', "\n";
$r4 = new ReflectionFunction('implode');
echo $r4->getNumberOfParameters(), "\n";
$r5 = new ReflectionFunction('max');
echo $r5->isVariadic() ? 'variadic' : 'fixed', "\n";
$pp = new ReflectionParameter('strpos', 'offset');
echo $pp->getPosition(), ':', json_encode($pp->getDefaultValue()), "\n";
--EXPECT--
1,1
string:string
3,2
string:req:string
offset:req:int
length:opt:?int
null
count:ref
2
variadic
2:0
