--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNodeList and DOMNamedNodeMap are IteratorAggregate, and attributes iterate by name
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r><x a="1" b="2"/><x/></r>');
$list = $d->getElementsByTagName('x');
$map  = $d->documentElement->firstChild->attributes;

var_dump($list instanceof Iterator, $list instanceof IteratorAggregate,
         $list instanceof Traversable, $list instanceof Countable);
var_dump($map instanceof IteratorAggregate, $map instanceof Countable);
var_dump(get_class($list->getIterator()), get_class($map->getIterator()));

// A node list keys by position...
$seen = [];
foreach ($list as $k => $n) { $seen[] = $k . ':' . $n->nodeName; }
var_dump($seen);

// ...an attribute map by attribute name.
$seen = [];
foreach ($map as $k => $a) { $seen[] = $k . '=' . $a->value; }
var_dump($seen);

// Each getIterator() is a fresh cursor, so two walks do not share a position.
$a = $list->getIterator();
$b = $list->getIterator();
$a->next();
var_dump($a->key(), $b->key());
var_dump(count($map), $map->length, count($list), $list->length);
--EXPECT--
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(16) "InternalIterator"
string(16) "InternalIterator"
array(2) {
  [0]=>
  string(3) "0:x"
  [1]=>
  string(3) "1:x"
}
array(2) {
  [0]=>
  string(3) "a=1"
  [1]=>
  string(3) "b=2"
}
int(1)
int(0)
int(2)
int(2)
int(2)
int(2)
