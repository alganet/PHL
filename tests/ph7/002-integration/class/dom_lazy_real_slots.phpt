--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Three properties of php's modern DOM tree are real slots filled on the first read
--FILE--
<?php
/* php declares `Dom\Element::$children`, `Dom\Element::$classList` and
 * `Dom\Document::$implementation` as REAL typed slots, not as the virtual
 * names every other property of that tree is -- and it leaves them
 * UNINITIALIZED until something reads one, filling the slot from its own
 * read handler. Five faces follow from that and from nothing else:
 *
 *  - Reflection reports them not virtual (modifiers 1, not 1|512).
 *  - `get_object_vars()`, the array cast, a walk and json_encode() answer
 *    the name only AFTER a read, and in the class's DECLARATION order
 *    rather than the order the reads happened in.
 *  - isset() is a read: it fills the slot like any other access.
 *  - the debug dump reads through the same handler, so var_dump() fills
 *    them too -- and a SECOND dump prints them first, out of the object's
 *    own table, ahead of the fabricated rows.
 *  - a clone starts over: php builds a new object and its slots are
 *    unfilled again, whatever the source had read.
 */
$mk = fn() => Dom\XMLDocument::createFromString('<r class="a b"><p><t>x</t></p></r>');

echo "-- declared, not virtual\n";
foreach ([['Dom\Element','children'],['Dom\Element','classList'],
          ['Dom\Document','children'],['Dom\Document','implementation'],
          ['Dom\DocumentFragment','children']] as [$cls,$prop]) {
    $r = new ReflectionProperty($cls,$prop);
    printf("%-22s %-15s mods=%d virtual=%s type=%s\n",
        $cls,$prop,$r->getModifiers(),var_export($r->isVirtual(),true),(string)$r->getType());
}

echo "\n-- unfilled until read\n";
$e = $mk()->documentElement;
var_dump(get_object_vars($e));
$one = $e->classList;
var_dump(array_keys(get_object_vars($e)));
$two = $e->children;
var_dump(array_keys(get_object_vars($e)));

echo "\n-- declaration order, not read order\n";
$d = $mk();
$x = $d->implementation;
$y = $d->children;
var_dump(array_keys(get_object_vars($d)));
var_dump(array_keys((array)$d));

echo "\n-- the slot is what answers afterwards\n";
var_dump($e->classList === $one, $e->children === $two);
foreach ($e as $k => $v) { echo "walk: $k\n"; }
echo json_encode($mk()->createDocumentFragment()), "\n";

echo "\n-- isset() is a read\n";
$i = $mk()->documentElement;
var_dump(isset($i->children), array_keys(get_object_vars($i)));

echo "\n-- the debug dump reads them too\n";
$g = $mk()->documentElement;
ob_start(); var_dump($g); ob_end_clean();
var_dump(array_keys(get_object_vars($g)));
ob_start(); var_dump($g); $second = ob_get_clean();
preg_match_all('/^  \["([^"]+)"\]/m', $second, $m);
var_dump(array_slice($m[1], 0, 4));

echo "\n-- a clone starts over\n";
$c = $mk()->documentElement;
$z = $c->children;
var_dump(array_keys(get_object_vars(clone $c)));

echo "\n-- and the handler still owns the write\n";
$w = $mk()->documentElement;
try { $w->children = 1; } catch (Throwable $t) { echo get_class($t),": ",$t->getMessage(),"\n"; }
try { unset($w->classList); } catch (Throwable $t) { echo get_class($t),": ",$t->getMessage(),"\n"; }
$r = $w->children;
try { $w->children = 1; } catch (Throwable $t) { echo get_class($t),": ",$t->getMessage(),"\n"; }
var_dump(property_exists($w,'implementation'), property_exists($w,'children'));
?>
--EXPECT--
-- declared, not virtual
Dom\Element            children        mods=1 virtual=false type=Dom\HTMLCollection
Dom\Element            classList       mods=1 virtual=false type=Dom\TokenList
Dom\Document           children        mods=1 virtual=false type=Dom\HTMLCollection
Dom\Document           implementation  mods=1 virtual=false type=Dom\Implementation
Dom\DocumentFragment   children        mods=1 virtual=false type=Dom\HTMLCollection

-- unfilled until read
array(0) {
}
array(1) {
  [0]=>
  string(9) "classList"
}
array(2) {
  [0]=>
  string(8) "children"
  [1]=>
  string(9) "classList"
}

-- declaration order, not read order
array(2) {
  [0]=>
  string(8) "children"
  [1]=>
  string(14) "implementation"
}
array(2) {
  [0]=>
  string(8) "children"
  [1]=>
  string(14) "implementation"
}

-- the slot is what answers afterwards
bool(true)
bool(true)
walk: children
walk: classList
{}

-- isset() is a read
bool(true)
array(1) {
  [0]=>
  string(8) "children"
}

-- the debug dump reads them too
array(2) {
  [0]=>
  string(8) "children"
  [1]=>
  string(9) "classList"
}
array(4) {
  [0]=>
  string(8) "children"
  [1]=>
  string(9) "classList"
  [2]=>
  string(12) "namespaceURI"
  [3]=>
  string(6) "prefix"
}

-- a clone starts over
array(0) {
}

-- and the handler still owns the write
Error: Cannot modify readonly property Dom\Element::$children
Error: Cannot unset Dom\Element::$classList
Error: Cannot modify readonly property Dom\Element::$children
bool(false)
bool(true)
