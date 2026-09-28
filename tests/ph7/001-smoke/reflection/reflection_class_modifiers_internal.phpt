--TEST--
Reflection: php's final internal classes, and every interface is abstract
--FILE--
<?php
// Two class-level facts this engine did not carry. Five internal classes are
// FINAL in php and were extendable here, two of them are refused to `new`
// outright, and `isAbstract()` answered false for every INTERFACE where zend
// gives one the implicit abstract bit -- unless it declares no method at all,
// which is why Traversable is the exception.
foreach (['Fiber', 'FiberError', 'Generator', 'DOMException', '__PHP_Incomplete_Class',
          'ArrayObject', 'Exception'] as $reflMC) {
    $reflMR = new ReflectionClass($reflMC);
    printf("%-24s final=%d abstract=%d modifiers=%d\n", $reflMC,
        (int)$reflMR->isFinal(), (int)$reflMR->isAbstract(), $reflMR->getModifiers());
}
foreach (['Traversable', 'Iterator', 'IteratorAggregate', 'Countable', 'Stringable',
          'Throwable', 'UnitEnum', 'BackedEnum', 'JsonSerializable', 'Reflector'] as $reflMC) {
    printf("%-20s abstract=%d modifiers=%d\n", $reflMC,
        (int)(new ReflectionClass($reflMC))->isAbstract(),
        (new ReflectionClass($reflMC))->getModifiers());
}
// A userland interface follows the same rule: the empty one is not abstract.
interface ReflMEmpty {}
interface ReflMOne { public function f(); }
interface ReflMTwo extends ReflMOne {}
foreach (['ReflMEmpty', 'ReflMOne', 'ReflMTwo'] as $reflMC) {
    printf("%-12s abstract=%d\n", $reflMC, (int)(new ReflectionClass($reflMC))->isAbstract());
}
// The two php reserves for its own use.
foreach (['Generator', 'FiberError'] as $reflMC) {
    try { (new ReflectionClass($reflMC))->newInstanceArgs([]); echo "$reflMC: built\n"; }
    catch (Throwable $reflME) { echo get_class($reflME), ': ', $reflME->getMessage(), "\n"; }
}
echo strtok((string)new ReflectionClass('Fiber'), "\n"), "\n";
echo strtok((string)new ReflectionClass('Countable'), "\n"), "\n";
--EXPECT--
Fiber                    final=1 abstract=0 modifiers=32
FiberError               final=1 abstract=0 modifiers=32
Generator                final=1 abstract=0 modifiers=32
DOMException             final=1 abstract=0 modifiers=32
__PHP_Incomplete_Class   final=1 abstract=0 modifiers=32
ArrayObject              final=0 abstract=0 modifiers=0
Exception                final=0 abstract=0 modifiers=0
Traversable          abstract=0 modifiers=0
Iterator             abstract=1 modifiers=0
IteratorAggregate    abstract=1 modifiers=0
Countable            abstract=1 modifiers=0
Stringable           abstract=1 modifiers=0
Throwable            abstract=1 modifiers=0
UnitEnum             abstract=1 modifiers=0
BackedEnum           abstract=1 modifiers=0
JsonSerializable     abstract=1 modifiers=0
Reflector            abstract=1 modifiers=0
ReflMEmpty   abstract=0
ReflMOne     abstract=1
ReflMTwo     abstract=1
Error: The "Generator" class is reserved for internal use and cannot be manually instantiated
Error: The "FiberError" class is reserved for internal use and cannot be manually instantiated
Class [ <internal:Core> final class Fiber ] {
Interface [ <internal:Core> interface Countable ] {
