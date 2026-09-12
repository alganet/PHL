--TEST--
ReflectionFunction over a Closure::fromCallable([obj,'m']) / fromCallable($invokeObj) resolves the underlying method signature (variadic, params)
--FILE--
<?php
class RvCalc {
    public function add(int $a, int $b = 5): int { return $a + $b; }
    public function __invoke(string $s, int ...$n) { return $s . array_sum($n); }
}
$rvO = new RvCalc();
$rvR1 = new ReflectionFunction(Closure::fromCallable([$rvO, 'add']));
echo var_export($rvR1->isVariadic(), true), '|', $rvR1->getNumberOfParameters(), '|', $rvR1->getNumberOfRequiredParameters(), "\n";
$rvR2 = new ReflectionFunction(Closure::fromCallable($rvO));
echo var_export($rvR2->isVariadic(), true), '|', $rvR2->getNumberOfParameters(), "\n";
$rvNames = array_map(fn($p) => $p->getName(), $rvR1->getParameters());
echo implode(',', $rvNames), "\n";
--EXPECT--
false|2|1
true|2
a,b
