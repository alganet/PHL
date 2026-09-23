--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SplObjectStorage keys by getHash, restarts its walk on detach and serializes both ways
--DESCRIPTION--
php's spl_SplObjectStorage is a table of {obj, inf} pairs plus TWO cursors that are
not the same thing: a position that walks the table and an index that key() reports.
Membership changes reset one or both — detach() restarts the walk, addAll() only
resets the index — and the embedded PHP, which kept a single integer offset, had
none of it. It was also missing six methods and two interfaces of a 25-method class:
seek() with SeekableIterator, serialize()/unserialize() with Serializable, the
__serialize()/__unserialize() pair php actually uses, and __debugInfo(). Two more
the model hides: current() RAISES on an invalid iterator where the chunk answered
null, and an overridden getHash() is what keys the table — the chunk called
spl_object_id() directly, so overriding it did nothing at all. attach/detach/
contains are exercised through their ArrayAccess aliases here, which run the same
C bodies: php DEPRECATES those three names since 8.5 and PHL keeps them working
silently, so calling them by name is the one thing this file cannot pin.
--FILE--
<?php
function sosShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', preg_replace('/#\d+/', '#N', str_replace("\n", '', $out)), "\n";
}
class SosItem {
    public $n;
    public function __construct($n) { $this->n = $n; }
}
function sosThree() {
    $s = new SplObjectStorage;
    foreach ([1, 2, 3] as $n) { $s[new SosItem($n)] = "i$n"; }
    return $s;
}
function sosWalk(SplObjectStorage $s) {
    $out = [];
    foreach ($s as $k => $v) { $out[] = "$k:{$v->n}:" . var_export($s->getInfo(), true); }
    return $out;
}

/* The presentation is php's: ONE entry, the storage, under a MANGLED private key,
 * and nothing at all for the (array) cast or var_export. */
$sosOne = new SplObjectStorage;
$sosOne[new SosItem(9)] = 'info';
ob_start(); var_dump($sosOne); $sosDump = trim(ob_get_clean());
echo preg_replace('/#\d+/', '#N', str_replace("\n", ' ', $sosDump)), "\n";
ob_start(); print_r($sosOne); $sosPrint = trim(ob_get_clean());
echo preg_replace('/#\d+/', '#N', str_replace("\n", ' ', $sosPrint)), "\n";
sosShow('debugInfo keys', fn() => array_keys($sosOne->__debugInfo()));
sosShow('cast is empty', fn() => (array)$sosOne);
sosShow('object vars are empty', fn() => get_object_vars($sosOne));
sosShow('no declared properties', fn() => (new ReflectionClass('SplObjectStorage'))->getProperties());
sosShow('export shows nothing', fn() => var_export($sosOne, true));

/* The two cursors. key() is an index of its own; next() advances it past the end,
 * detach() restarts the walk, and addAll() resets only the index. */
sosShow('walk', fn() => sosWalk(sosThree()));
sosShow('key past the end', function () {
    $s = new SplObjectStorage; $s[new SosItem(1)] = 'x';
    $s->rewind(); $s->next();
    return [$s->key(), $s->valid()];
});
sosShow('current on an invalid iterator', function () {
    $s = new SplObjectStorage; $s->rewind(); return $s->current();
});
sosShow('getInfo past the end is null', function () {
    $s = new SplObjectStorage; $s->rewind(); return $s->getInfo();
});
sosShow('detach restarts the walk', function () {
    $s = sosThree(); $s->rewind(); $s->next(); $s->next();
    $s->offsetUnset($s->current());
    return [$s->key(), $s->valid(), $s->current()->n];
});
sosShow('seek', function () {
    $s = sosThree(); $s->seek(2);
    return [$s->key(), $s->current()->n, $s->getInfo()];
});
sosShow('seek out of range', fn() => sosThree()->seek(9));
sosShow('seek refuses a non-numeric string', fn() => sosThree()->seek('x'));
sosShow('setInfo writes the current pair', function () {
    $s = sosThree(); $s->rewind(); $s->next(); $s->setInfo('EDITED');
    return sosWalk($s);
});

/* The three set operations answer the new count; two of them restart the walk. */
sosShow('addAll', function () {
    $a = sosThree(); $b = new SplObjectStorage; $b[new SosItem(4)] = 'i4';
    return [$a->addAll($b), count($a), $a->key()];
});
sosShow('addAll overwrites the info', function () {
    $o = new SosItem(1); $a = new SplObjectStorage; $a[$o] = 'old';
    $b = new SplObjectStorage; $b[$o] = 'new'; $a->addAll($b);
    return [count($a), $a[$o]];
});
sosShow('removeAll', function () {
    $a = sosThree(); $a->rewind(); $b = new SplObjectStorage; $b->addAll($a);
    return [$a->removeAll($b), count($a)];
});
sosShow('removeAllExcept', function () {
    $a = sosThree(); $a->rewind(); $a->next();
    $keep = new SplObjectStorage; $keep[$a->current()] = null;
    return [$a->removeAllExcept($keep), count($a), $a->current()->n];
});
sosShow('addAll wants a storage', fn() => (new SplObjectStorage)->addAll([1]));

/* php's ZPP: the four ArrayAccess offsets are UNTYPED in the stub and take an
 * object anyway, and the refusal names THIS class even from a subclass. */
class SosSub extends SplObjectStorage {}
sosShow('an offset refuses a scalar', fn() => (new SplObjectStorage)->offsetGet(1));
sosShow('a subclass refuses it the same way', fn() => (new SosSub)->offsetGet(1));
sosShow('an offset refuses it too', function () { $s = new SplObjectStorage; $s['k'] = 1; });
sosShow('a missing offset raises', fn() => (new SplObjectStorage)[new SosItem(1)]);
sosShow('contains answers a stored null', function () {
    $o = new SosItem(1); $s = new SplObjectStorage; $s[$o] = null;
    return [$s->offsetExists($o), isset($s[$o]), empty($s[$o])];
});

/* An overridden getHash() KEYS the table: two objects that hash alike are one entry. */
class SosByParity extends SplObjectStorage {
    public function getHash(object $object): string { return 'p' . ($object->n % 2); }
}
sosShow('an overridden getHash keys the storage', function () {
    $s = new SosByParity;
    $s[new SosItem(1)] = 'odd'; $s[new SosItem(3)] = 'also odd'; $s[new SosItem(2)] = 'even';
    return [count($s), sosWalk($s)];
});
sosShow('getHash is spl_object_hash', function () {
    $o = new SosItem(1);
    return (new SplObjectStorage)->getHash($o) === spl_object_hash($o);
});

/* All four serialization entry points. */
sosShow('__serialize', function () {
    $s = new SplObjectStorage; $s[new SosItem(1)] = 'i';
    return $s->__serialize();
});
sosShow('serialize', function () {
    $s = new SplObjectStorage; $s[new SosItem(1)] = 'i';
    return serialize($s);
});
sosShow('round trip', function () {
    $s = sosThree();
    return sosWalk(unserialize(serialize($s)));
});
sosShow('the legacy Serializable format', function () {
    $s = new SplObjectStorage; $s[new SosItem(1)] = 'i';
    return $s->serialize();
});
sosShow('the legacy format reads back', function () {
    $s = sosThree(); $n = new SplObjectStorage;
    $n->unserialize($s->serialize());
    return sosWalk($n);
});
sosShow('a corrupt legacy payload names the offset',
    fn() => (new SplObjectStorage)->unserialize('x:s:1:"a";m:a:0:{}'));
sosShow('an empty legacy payload is a no-op', function () {
    $s = new SplObjectStorage; $s->unserialize(''); return count($s);
});
sosShow('__unserialize', function () {
    $s = new SplObjectStorage; $s->__unserialize([[new SosItem(1), 'i'], []]);
    return [count($s), $s->getInfo() ?? 'unpositioned'];
});
sosShow('__unserialize wants pairs',
    fn() => (new SplObjectStorage)->__unserialize([[new SosItem(1)], []]));
sosShow('__unserialize wants objects',
    fn() => (new SplObjectStorage)->__unserialize([[1, 'i'], []]));
sosShow('__unserialize wants both halves',
    fn() => (new SplObjectStorage)->__unserialize([1, 2]));

/* The class shape, and the two methodless interfaces php declares beside it. */
sosShow('methods', fn() => get_class_methods('SplObjectStorage'));
sosShow('SplObserver', fn() => get_class_methods('SplObserver'));
sosShow('SplSubject', fn() => get_class_methods('SplSubject'));
sosShow('the observer contract is usable', function () {
    $subject = new class implements SplSubject {
        public $seen = 0;
        private $obs;
        public function __construct() { $this->obs = new SplObjectStorage; }
        public function attach(SplObserver $observer): void { $this->obs[$observer] = null; }
        public function detach(SplObserver $observer): void { unset($this->obs[$observer]); }
        public function notify(): void { foreach ($this->obs as $o) { $o->update($this); } }
    };
    $subject->attach(new class implements SplObserver {
        public function update(SplSubject $subject): void { $subject->seen++; }
    });
    $subject->notify();
    return $subject->seen;
});
--EXPECT--
object(SplObjectStorage)#N (1) {   ["storage":"SplObjectStorage":private]=>   array(1) {     [0]=>     array(2) {       ["obj"]=>       object(SosItem)#N (1) {         ["n"]=>         int(9)       }       ["inf"]=>       string(4) "info"     }   } }
SplObjectStorage Object (     [storage:SplObjectStorage:private] => Array         (             [0] => Array                 (                     [obj] => SosItem Object                         (                             [n] => 9                         )                      [inf] => info                 )          )  )
debugInfo keys => array (  0 => '' . "\0" . 'SplObjectStorage' . "\0" . 'storage',)
cast is empty => array ()
object vars are empty => array ()
no declared properties => array ()
export shows nothing => '\\SplObjectStorage::__set_state(array())'
walk => array (  0 => '0:1:\'i1\'',  1 => '1:2:\'i2\'',  2 => '2:3:\'i3\'',)
key past the end => array (  0 => 1,  1 => false,)
current on an invalid iterator => RuntimeException: Called current() on invalid iterator
getInfo past the end is null => NULL
detach restarts the walk => array (  0 => 0,  1 => true,  2 => 1,)
seek => array (  0 => 2,  1 => 3,  2 => 'i3',)
seek out of range => OutOfBoundsException: Seek position 9 is out of range
seek refuses a non-numeric string => TypeError: SplObjectStorage::seek(): Argument #N ($offset) must be of type int, string given
setInfo writes the current pair => array (  0 => '0:1:\'i1\'',  1 => '1:2:\'EDITED\'',  2 => '2:3:\'i3\'',)
addAll => array (  0 => 4,  1 => 4,  2 => 0,)
addAll overwrites the info => array (  0 => 1,  1 => 'new',)
removeAll => array (  0 => 0,  1 => 0,)
removeAllExcept => array (  0 => 1,  1 => 1,  2 => 2,)
addAll wants a storage => TypeError: SplObjectStorage::addAll(): Argument #N ($storage) must be of type SplObjectStorage, array given
an offset refuses a scalar => TypeError: SplObjectStorage::offsetGet(): Argument #N ($object) must be of type object, int given
a subclass refuses it the same way => TypeError: SplObjectStorage::offsetGet(): Argument #N ($object) must be of type object, int given
an offset refuses it too => TypeError: SplObjectStorage::offsetSet(): Argument #N ($object) must be of type object, string given
a missing offset raises => UnexpectedValueException: Object not found
contains answers a stored null => array (  0 => true,  1 => true,  2 => true,)
an overridden getHash keys the storage => array (  0 => 2,  1 =>   array (    0 => '0:1:\'also odd\'',    1 => '1:2:\'even\'',  ),)
getHash is spl_object_hash => true
__serialize => array (  0 =>   array (    0 =>     \SosItem::__set_state(array(       'n' => 1,    )),    1 => 'i',  ),  1 =>   array (  ),)
serialize => 'O:16:"SplObjectStorage":2:{i:0;a:2:{i:0;O:7:"SosItem":1:{s:1:"n";i:1;}i:1;s:1:"i";}i:1;a:0:{}}'
round trip => array (  0 => '0:1:\'i1\'',  1 => '1:2:\'i2\'',  2 => '2:3:\'i3\'',)
the legacy Serializable format => 'x:i:1;O:7:"SosItem":1:{s:1:"n";i:1;},s:1:"i";;m:a:0:{}'
the legacy format reads back => array (  0 => '0:1:\'i1\'',  1 => '1:2:\'i2\'',  2 => '2:3:\'i3\'',)
a corrupt legacy payload names the offset => UnexpectedValueException: Error at offset 10 of 18 bytes
an empty legacy payload is a no-op => 0
__unserialize => array (  0 => 1,  1 => 'i',)
__unserialize wants pairs => UnexpectedValueException: Odd number of elements
__unserialize wants objects => UnexpectedValueException: Non-object key
__unserialize wants both halves => UnexpectedValueException: Incomplete or ill-typed serialization data
methods => array (  0 => 'attach',  1 => 'detach',  2 => 'contains',  3 => 'addAll',  4 => 'removeAll',  5 => 'removeAllExcept',  6 => 'getInfo',  7 => 'setInfo',  8 => 'count',  9 => 'rewind',  10 => 'valid',  11 => 'key',  12 => 'current',  13 => 'next',  14 => 'seek',  15 => 'unserialize',  16 => 'serialize',  17 => 'offsetExists',  18 => 'offsetGet',  19 => 'offsetSet',  20 => 'offsetUnset',  21 => 'getHash',  22 => '__serialize',  23 => '__unserialize',  24 => '__debugInfo',)
SplObserver => array (  0 => 'update',)
SplSubject => array (  0 => 'attach',  1 => 'detach',  2 => 'notify',)
the observer contract is usable => 1
