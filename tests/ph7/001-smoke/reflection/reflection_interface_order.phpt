--TEST--
Reflection: the interface list is in php's order, internal and compiled
--FILE--
<?php
// zend builds the list at link time out of two primitives, and every rule
// below is one of them: a list is copied in BACKWARDS, an INTERNAL class hands
// its interfaces over one at a time (so each is followed by its own), and a
// compiled one declares them as a BLOCK. This engine walked a flattened set of
// its own and matched php on none of them.
interface ReflOA {} interface ReflOB extends ReflOA {} interface ReflOC {}
interface ReflOD extends ReflOC, ReflOB {}
interface ReflOE extends ReflOD {}
class ReflOP implements ReflOB {}
class ReflOQ extends ReflOP implements ReflOC {}
class ReflOR extends ReflOP {}                    // declares none: copied BACKWARDS
class ReflOS implements ReflOD, ReflOA {}
class ReflOT implements ReflOC, ReflOD {}
class ReflOU implements ReflOC { public function __toString(): string { return ''; } }
foreach (['ReflOD','ReflOE','ReflOP','ReflOQ','ReflOR','ReflOS','ReflOT','ReflOU'] as $reflOX) {
    printf("%-7s %s\n", $reflOX, implode(', ', (new ReflectionClass($reflOX))->getInterfaceNames()));
}
// and the internal shapes the rule was measured on
foreach (['SplFixedArray', 'DOMNodeList', 'RecursiveIteratorIterator', 'CachingIterator',
          'RecursiveCachingIterator', 'SplFileObject', 'SplObjectStorage', 'SplStack',
          'Exception', 'TypeError', 'ArgumentCountError', 'ReflectionClass',
          'OuterIterator', 'PropertyHookType'] as $reflOX) {
    printf("%-26s %s\n", $reflOX, implode(', ', (new ReflectionClass($reflOX))->getInterfaceNames()));
}
// class_implements() and the export clause publish the very same order
print_r(class_implements('SplStack'));
echo strtok((string)new ReflectionClass('CachingIterator'), "\n"), "\n";
echo strtok((string)new ReflectionClass('ReflOD'), "\n"), "\n";
--EXPECT--
ReflOD  ReflOC, ReflOB, ReflOA
ReflOE  ReflOD, ReflOA, ReflOB, ReflOC
ReflOP  ReflOB, ReflOA
ReflOQ  ReflOB, ReflOA, ReflOC
ReflOR  ReflOA, ReflOB
ReflOS  ReflOD, ReflOA, ReflOB, ReflOC
ReflOT  ReflOC, ReflOD, ReflOA, ReflOB
ReflOU  ReflOC, Stringable
SplFixedArray              IteratorAggregate, Traversable, ArrayAccess, Countable, JsonSerializable
DOMNodeList                IteratorAggregate, Traversable, Countable
RecursiveIteratorIterator  OuterIterator, Traversable, Iterator
CachingIterator            Stringable, Iterator, Traversable, OuterIterator, ArrayAccess, Countable
RecursiveCachingIterator   Countable, ArrayAccess, OuterIterator, Traversable, Iterator, Stringable, RecursiveIterator
SplFileObject              Stringable, RecursiveIterator, Traversable, Iterator, SeekableIterator
SplObjectStorage           Countable, SeekableIterator, Traversable, Iterator, Serializable, ArrayAccess
SplStack                   Serializable, ArrayAccess, Countable, Traversable, Iterator
Exception                  Stringable, Throwable
TypeError                  Throwable, Stringable
ArgumentCountError         Stringable, Throwable
ReflectionClass            Stringable, Reflector
OuterIterator              Iterator, Traversable
PropertyHookType           BackedEnum, UnitEnum
Array
(
    [Serializable] => Serializable
    [ArrayAccess] => ArrayAccess
    [Countable] => Countable
    [Traversable] => Traversable
    [Iterator] => Iterator
)
Class [ <internal:SPL> <iterateable> class CachingIterator extends IteratorIterator implements Stringable, Iterator, Traversable, OuterIterator, ArrayAccess, Countable ] {
Interface [ <user> interface ReflOD extends ReflOC, ReflOB, ReflOA ] {
