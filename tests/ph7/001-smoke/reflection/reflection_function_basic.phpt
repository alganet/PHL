--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionFunction basics: params, invoke, closures, static vars
--FILE--
<?php
function reflFnAdd($a, $b = 10, ...$rest) {
    static $calls = 0;
    $calls++;
    return $a + $b + count($rest);
}
function &reflFnRef(array &$out) { return $out; }
function reflFnGen() { yield 1; }

$rf = new ReflectionFunction('reflFnAdd');
echo $rf->getName(), "\n";
echo $rf->getNumberOfParameters(), ',', $rf->getNumberOfRequiredParameters(), "\n";
echo $rf->isVariadic() ? 'variadic' : 'not-variadic', "\n";
echo $rf->isGenerator() ? 'gen' : 'not-gen', "\n";
echo $rf->returnsReference() ? 'byref' : 'byval', "\n";
echo $rf->isClosure() ? 'closure' : 'not-closure', "\n";
echo $rf->isAnonymous() ? 'anon' : 'named', "\n";
echo $rf->isInternal() ? 'internal' : 'user', "\n";
echo $rf->invoke(1, 2), "\n";
echo $rf->invokeArgs(array(1, 2, 3, 4)), "\n";
echo json_encode($rf->getStaticVariables()), "\n";
$c = $rf->getClosure();
echo get_class($c), ':', $c(5), "\n";

$rr = new ReflectionFunction('reflFnRef');
echo $rr->returnsReference() ? 'byref' : 'byval', "\n";
$rg = new ReflectionFunction('reflFnGen');
echo $rg->isGenerator() ? 'gen' : 'not-gen', "\n";

echo is_int($rf->getStartLine()) && $rf->getEndLine() >= $rf->getStartLine() ? 'lines-ok' : 'lines-bad', "\n";
echo basename($rf->getFileName()) === basename(__FILE__) ? 'file-ok' : 'file-bad', "\n";

$ri = new ReflectionFunction('strlen');
echo $ri->isInternal() ? 'internal' : 'user', "\n";
echo $ri->getFileName() === false ? 'no-file' : 'file', "\n";
echo $ri->invoke('hello'), "\n";

try {
    new ReflectionFunction('reflFnNoSuch');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
reflFnAdd
3,1
variadic
not-gen
byval
not-closure
named
user
3
5
{"calls":2}
Closure:15
byref
gen
lines-ok
file-ok
internal
no-file
5
ReflectionException: Function reflFnNoSuch() does not exist
