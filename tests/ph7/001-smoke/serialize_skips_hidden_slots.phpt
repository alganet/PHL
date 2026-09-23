--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
serialize() no longer emits a native class's HIDDEN engine slots
--DESCRIPTION--
PH7_CLASS_ATTR_HIDDEN takes a native class's engine state off eight presentation surfaces, and
serialize() was the ninth that could not join them: those slots are the only place the state
lives, so dropping them without php's replacement made unserialize(serialize($x)) answer an
EMPTY object. php's replacement is an __serialize()/__unserialize() pair, and every class php
round-trips now declares one here, so the filter can finally run.
What it fixes is the SPL DECORATOR family, whose state php does not round-trip either: php's
payload for an IteratorIterator is `0:{}`, and PHL emitted six mangled slots — including a full
copy of the inner iterator, twice. RegexIterator is the case that shows the filter is not
"drop everything": its `replacement` is a REAL php property and stays.
--FILE--
<?php
function serShow($label, $fn) {
    try { $out = $fn(); if (!is_string($out)) { $out = var_export($out, true); } }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\0", '^@', $out), "\n";
}
$src = new ArrayIterator([1]);

echo "-- the decorators carry no state into the payload\n";
serShow('IteratorIterator', fn() => serialize(new IteratorIterator($src)));
serShow('LimitIterator', fn() => serialize(new LimitIterator($src)));
serShow('InfiniteIterator', fn() => serialize(new InfiniteIterator($src)));
serShow('NoRewindIterator', fn() => serialize(new NoRewindIterator($src)));
serShow('EmptyIterator', fn() => serialize(new EmptyIterator));
serShow('AppendIterator', fn() => serialize(new AppendIterator));
serShow('RecursiveIteratorIterator',
    fn() => serialize(new RecursiveIteratorIterator(new RecursiveArrayIterator([1]))));

echo "-- but a REAL property is not a hidden slot\n";
serShow('RegexIterator', fn() => serialize(new RegexIterator($src, '/x/')));

echo "-- the classes that DO round-trip still do, through their pair\n";
serShow('ArrayIterator', fn() => serialize(new ArrayIterator([1, 'k' => 2])));
serShow('DateTimeZone', fn() => serialize(new DateTimeZone('UTC')));
serShow('SplStack', function () { $s = new SplStack; $s->push(7); return serialize($s); });
serShow('round trip still works', function () {
    $d = unserialize(serialize(new DateTime('2021-03-04 05:06:07', new DateTimeZone('UTC'))));
    return $d->format('c');
});
serShow('and so does the store', function () {
    $a = unserialize(serialize(new ArrayObject(['k' => 2])));
    return json_encode($a->getArrayCopy());
});

echo "-- a class holding engine state still refuses outright\n";
serShow('Closure', fn() => serialize(fn() => 1));
serShow('WeakReference', fn() => serialize(WeakReference::create(new stdClass)));
--EXPECT--
-- the decorators carry no state into the payload
IteratorIterator => O:16:"IteratorIterator":0:{}
LimitIterator => O:13:"LimitIterator":0:{}
InfiniteIterator => O:16:"InfiniteIterator":0:{}
NoRewindIterator => O:16:"NoRewindIterator":0:{}
EmptyIterator => O:13:"EmptyIterator":0:{}
AppendIterator => O:14:"AppendIterator":0:{}
RecursiveIteratorIterator => O:25:"RecursiveIteratorIterator":0:{}
-- but a REAL property is not a hidden slot
RegexIterator => O:13:"RegexIterator":1:{s:11:"replacement";N;}
-- the classes that DO round-trip still do, through their pair
ArrayIterator => O:13:"ArrayIterator":4:{i:0;i:0;i:1;a:2:{i:0;i:1;s:1:"k";i:2;}i:2;a:0:{}i:3;N;}
DateTimeZone => O:12:"DateTimeZone":2:{s:13:"timezone_type";i:3;s:8:"timezone";s:3:"UTC";}
SplStack => O:8:"SplStack":3:{i:0;i:6;i:1;a:1:{i:0;i:7;}i:2;a:0:{}}
round trip still works => 2021-03-04T05:06:07+00:00
and so does the store => {"k":2}
-- a class holding engine state still refuses outright
Closure => Exception: Serialization of 'Closure' is not allowed
WeakReference => Exception: Serialization of 'WeakReference' is not allowed
