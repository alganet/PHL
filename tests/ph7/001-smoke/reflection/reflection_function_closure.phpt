--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionFunction over closures: name, captures, invoke
--FILE--
<?php
$reflClA = 4;
$reflClosure = function ($x) use ($reflClA) { return $x + $reflClA; };

$rf = new ReflectionFunction($reflClosure);
echo strpos($rf->getName(), '{closure:') === 0 ? 'closure-name' : 'other-name', "\n";
echo $rf->isClosure() ? 'closure' : 'not-closure', "\n";
echo $rf->isAnonymous() ? 'anon' : 'named', "\n";
echo $rf->invoke(6), "\n";
echo json_encode($rf->getClosureUsedVariables()), "\n";
echo $rf->getClosureThis() === null ? 'no-this' : 'this', "\n";
echo $rf->getClosureScopeClass() === null ? 'no-scope' : 'scope', "\n";
echo $rf->getNumberOfParameters(), "\n";
$c2 = $rf->getClosure();
echo $c2(10), "\n";

$plain = function () { return 'plain'; };
$rp = new ReflectionFunction($plain);
echo $rp->isClosure() ? 'closure' : 'not-closure', "\n";
echo json_encode($rp->getClosureUsedVariables()), "\n";
echo $rp->invoke(), "\n";
?>
--EXPECT--
closure-name
closure
anon
10
{"reflClA":4}
no-this
no-scope
1
14
closure
[]
plain
