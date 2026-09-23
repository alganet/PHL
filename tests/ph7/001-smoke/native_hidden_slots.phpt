--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A native class's engine slots are invisible to every presentation surface
--DESCRIPTION--
php keeps a built-in class's state in its own C struct, so `getProperties()`,
`(array)`, `get_object_vars()`, `foreach`, `json_encode()` and `var_export()` all
report NOTHING for a Fiber, a WeakReference, an IteratorIterator or a DateTime.
PHL's native classes hold the same state in real declared slots, which every one
of those surfaces printed — including a raw POINTER for WeakReference, a value
that changes on every run. PH7_CLASS_ATTR_HIDDEN takes them off the presentation
surfaces while `new`, `clone` and the class's own C bodies keep using them;
serialize() deliberately still emits them, because they are the only place the
state lives and php replaces them with an __serialize pair PHL has not built.
--FILE--
<?php
function nhsShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
$nhsMk = [
    'IteratorIterator' => fn() => new IteratorIterator(new ArrayIterator([1, 2])),
    'LimitIterator'    => fn() => new LimitIterator(new ArrayIterator([1, 2]), 0, 1),
    'WeakReference'    => fn() => WeakReference::create(new stdClass()),
    'WeakMap'          => fn() => new WeakMap(),
    'XMLWriter'        => fn() => new XMLWriter(),
    'DateTime'         => fn() => new DateTime('2020-01-02 03:04:05', new DateTimeZone('UTC')),
    'DateTimeZone'     => fn() => new DateTimeZone('UTC'),
];

/* Reflection reports no property at all for any of them. */
nhsShow('reflected', fn() => array_map(
    fn($c) => count((new ReflectionClass($c))->getProperties()),
    array_keys($nhsMk)));

/* Neither do the raw object surfaces. Only the KEYS are compared: php fills some
 * of them from a debug-info handler PHL has no equivalent for, so the values are
 * not the same — the point is that no PHL-only slot name is among them. */
nhsShow('cast keys', fn() => array_values(array_filter(array_merge(...array_map(
    fn($f) => array_keys((array)$f()), array_values($nhsMk))),
    fn($k) => str_contains((string)$k, '__'))));
nhsShow('object vars', fn() => array_values(array_filter(array_merge(...array_map(
    fn($f) => array_keys(get_object_vars($f())), array_values($nhsMk))),
    fn($k) => str_contains((string)$k, '__'))));
nhsShow('foreach keys', function () use ($nhsMk) {
    $seen = [];
    foreach ($nhsMk as $f) { foreach ($f() as $k => $v) { $seen[] = (string)$k; } }
    return array_values(array_filter($seen, fn($k) => str_contains($k, '__')));
});
nhsShow('json', fn() => array_values(array_filter(array_map(
    fn($f) => json_encode($f()), array_values($nhsMk)),
    fn($j) => str_contains($j, '__'))));
/* `'__` and not `__`: every export opens with the class's own __set_state(). */
nhsShow('export', fn() => array_values(array_filter(array_map(
    fn($f) => var_export($f(), true), array_values($nhsMk)),
    fn($j) => str_contains($j, "'__"))));

/* The slots are still THERE: the class's own methods read them, `new` fills them
 * and `clone` copies them. */
nhsShow('methods still work', function () {
    $l = new LimitIterator(new ArrayIterator([0, 1, 2, 3]), 1, 2);
    $out = [];
    foreach ($l as $v) { $out[] = $v; }
    return [$out, $l->getPosition(), $l->getInnerIterator() instanceof ArrayIterator];
});
nhsShow('clone copies', function () {
    $a = new ArrayObject(['x' => 1]);
    $b = clone $a;
    $b['y'] = 2;
    return [$a->getArrayCopy(), $b->getArrayCopy()];
});
nhsShow('serialize round-trips', function () {
    $d = unserialize(serialize(new DateTime('2020-01-02 03:04:05', new DateTimeZone('UTC'))));
    $i = unserialize(serialize(new ArrayIterator(['k' => 1])));
    return [$d->format('Y-m-d H:i:s'), $i->getArrayCopy()];
});
nhsShow('weakref still resolves', function () {
    $o = new stdClass();
    $r = WeakReference::create($o);
    return [$r->get() === $o, WeakReference::create($o) === $r];
});
--EXPECT--
reflected => array (  0 => 0,  1 => 0,  2 => 0,  3 => 0,  4 => 0,  5 => 0,  6 => 0,)
cast keys => array ()
object vars => array ()
foreach keys => array ()
json => array ()
export => array ()
methods still work => array (  0 =>   array (    0 => 1,    1 => 2,  ),  1 => 3,  2 => true,)
clone copies => array (  0 =>   array (    'x' => 1,  ),  1 =>   array (    'x' => 1,    'y' => 2,  ),)
serialize round-trips => array (  0 => '2020-01-02 03:04:05',  1 =>   array (    'k' => 1,  ),)
weakref still resolves => array (  0 => true,  1 => true,)
