--TEST--
ArrayObject / ArrayIterator implement Serializable, in php's byte format
--FILE--
<?php
// php declares `Serializable` on both classes and answers its two names with a
// LEGACY byte format the magic pair replaced -- `x:<flags><storage>;m:<members>`,
// each part php's own serialize() output. Neither name existed here, so the
// interface could not be declared: `$ao instanceof Serializable` was false and
// `$ao->serialize()` a Call to undefined method.
$splSA = new ArrayObject([1, 2, 'k' => 'v']);
echo var_export($splSA->serialize(), true), "\n";
echo var_export((new ArrayIterator([1, 2]))->serialize(), true), "\n";
class SplSSub extends ArrayObject { public $x = 5; }
echo var_export((new SplSSub([1]))->serialize(), true), "\n";
echo var_export((new RecursiveArrayIterator([1]))->serialize(), true), "\n";
var_dump($splSA instanceof Serializable, (new ArrayIterator) instanceof Serializable);

// A round trip, and the members with it. The member is DECLARED: php 8.2
// deprecates creating a dynamic property, and this is not the row for it.
class SplSMem extends ArrayObject { public $p = 0; }
$splSB = new SplSMem();
$splSB->unserialize('x:i:2;a:1:{i:0;s:1:"z";};m:a:1:{s:1:"p";i:7;}');
var_dump($splSB->getArrayCopy(), $splSB->getFlags(), $splSB->p);
// The magic pair reads the same members back.
$splSC = new SplSMem();
$splSC->__unserialize([0, ['z'], ['p' => 7], null]);
var_dump($splSC->p);

// php's reader reports WHERE it gave up: a value it could not read at all
// blames where it started, one it read and rejected for its TYPE blames after
// it, and the storage is screened by its type BYTE before any read.
foreach (['', 'junk', 'x', 'xy', 'x:', 'x:i:0', 'x:i:0;', 'x:i:0;a:0:{}', 'x:i:0;a:0:{};',
          'x:i:0;a:0:{};m', 'x:i:0;a:0:{};m:', 'x:i:0;a:0:{};m:i:1;',
          'x:s:1:"a";a:0:{};m:a:0:{}', 'x:i:0;i:5;m:a:0:{}', 'x:i:0;m:a:0:{}',
          'x:i:0;N;m:a:0:{}',
          // (an OBJECT backing array reads too, and is left out here: php 8.5
          // deprecates it, which is a policy item of its own)
          'x:i:0;a:0:{};m:a:0:{}extra'] as $splSPayload) {
    $splSD = new ArrayObject(['keep' => 1]);
    try {
        $splSD->unserialize($splSPayload);
        echo var_export($splSPayload, true), ' => OK ', json_encode($splSD->getArrayCopy()), "\n";
    } catch (Throwable $splSE) {
        echo var_export($splSPayload, true), ' => ', get_class($splSE), ': ', $splSE->getMessage(), "\n";
    }
}
echo trim(strtok((string)new ReflectionMethod('ArrayObject', 'serialize'), "\n")), "\n";
echo strtok((string)new ReflectionClass('ArrayObject'), "\n"), "\n";
--EXPECT--
'x:i:0;a:3:{i:0;i:1;i:1;i:2;s:1:"k";s:1:"v";};m:a:0:{}'
'x:i:0;a:2:{i:0;i:1;i:1;i:2;};m:a:0:{}'
'x:i:0;a:1:{i:0;i:1;};m:a:1:{s:1:"x";i:5;}'
'x:i:0;a:1:{i:0;i:1;};m:a:0:{}'
bool(true)
bool(true)
array(1) {
  [0]=>
  string(1) "z"
}
int(2)
int(7)
int(7)
'' => OK {"keep":1}
'junk' => UnexpectedValueException: Error at offset 0 of 4 bytes
'x' => UnexpectedValueException: Error at offset 1 of 1 bytes
'xy' => UnexpectedValueException: Error at offset 1 of 2 bytes
'x:' => UnexpectedValueException: Error at offset 2 of 2 bytes
'x:i:0' => UnexpectedValueException: Error at offset 2 of 5 bytes
'x:i:0;' => UnexpectedValueException: Error at offset 6 of 6 bytes
'x:i:0;a:0:{}' => UnexpectedValueException: Error at offset 12 of 12 bytes
'x:i:0;a:0:{};' => UnexpectedValueException: Error at offset 13 of 13 bytes
'x:i:0;a:0:{};m' => UnexpectedValueException: Error at offset 14 of 14 bytes
'x:i:0;a:0:{};m:' => UnexpectedValueException: Error at offset 15 of 15 bytes
'x:i:0;a:0:{};m:i:1;' => UnexpectedValueException: Error at offset 19 of 19 bytes
'x:s:1:"a";a:0:{};m:a:0:{}' => UnexpectedValueException: Error at offset 10 of 25 bytes
'x:i:0;i:5;m:a:0:{}' => UnexpectedValueException: Error at offset 6 of 18 bytes
'x:i:0;m:a:0:{}' => UnexpectedValueException: Error at offset 6 of 14 bytes
'x:i:0;N;m:a:0:{}' => UnexpectedValueException: Error at offset 6 of 16 bytes
'x:i:0;a:0:{};m:a:0:{}extra' => OK []
Method [ <internal:SPL, prototype Serializable> public method serialize ] {
Class [ <internal:SPL> <iterateable> class ArrayObject implements IteratorAggregate, Traversable, ArrayAccess, Serializable, Countable ] {
