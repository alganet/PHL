--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a user subclass of an uncloneable class is uncloneable: the refusal is the inherited handler, named after the subclass, and __clone changes nothing
--FILE--
<?php
/* the refusal walks the base chain and prints the INSTANCE's class */
class UncloneSubEx extends Exception {}
class UncloneSubIt extends IteratorIterator {}
class UncloneSubXw extends XMLWriter {}
$w = new UncloneSubXw;
$w->openMemory();
foreach ([new UncloneSubEx('m'), new UncloneSubIt(new ArrayIterator([1])), $w] as $o) {
    try { clone $o; echo "cloned\n"; }
    catch (Error $e) { echo $e->getMessage(), "\n"; }
}

/* a subclass declaring its own PUBLIC __clone is refused all the same: php
 * never reaches the method, the handler answers first */
class UncloneSubOwn extends Exception { public function __clone(): void { echo "ran\n"; } }
try { clone new UncloneSubOwn('m'); }
catch (Error $e) { echo $e->getMessage(), "\n"; }

/* clone() the function takes the same door */
try { clone(new UncloneSubEx('m')); }
catch (Error $e) { echo $e->getMessage(), "\n"; }

/* Reflection: isCloneable answers false up the chain too */
foreach (['Exception', 'UncloneSubEx', 'IteratorIterator', 'UncloneSubIt', 'UncloneSubOwn', 'stdClass'] as $cn) {
    var_dump($cn, (new ReflectionClass($cn))->isCloneable());
}

/* an ordinary subclass of a CLONEABLE class still clones */
class UncloneSubPlain extends ArrayObject {}
$p = new UncloneSubPlain([1, 2]);
$c = clone $p;
var_dump(get_class($c), $c !== $p, count($c));
--EXPECT--
Trying to clone an uncloneable object of class UncloneSubEx
Trying to clone an uncloneable object of class UncloneSubIt
Trying to clone an uncloneable object of class UncloneSubXw
Trying to clone an uncloneable object of class UncloneSubOwn
Trying to clone an uncloneable object of class UncloneSubEx
string(9) "Exception"
bool(false)
string(12) "UncloneSubEx"
bool(false)
string(16) "IteratorIterator"
bool(false)
string(12) "UncloneSubIt"
bool(false)
string(13) "UncloneSubOwn"
bool(true)
string(8) "stdClass"
bool(true)
string(15) "UncloneSubPlain"
bool(true)
int(2)
