--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNodeList is live: getElementsByTagName reflects tree mutations
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r><x i="1"/><x i="2"/><x i="3"/></r>');
$live = $d->getElementsByTagName('x');
var_dump($live->length, count($live));
$snap = [];
foreach ($live as $k => $n) { $snap[] = $k . ':' . $n->getAttribute('i'); }
var_dump($snap);
$nodes = [];
foreach ($live as $n) { $nodes[] = $n; }
foreach ($nodes as $n) { $n->parentNode->removeChild($n); }
var_dump($live->length);
var_dump($live->item(0));
$all = $d->getElementsByTagName('*');
var_dump($all->length);
$d2 = new DOMDocument; $d2->preserveWhiteSpace = false;
$d2->loadXML('<a><b><c/><c/></b><c/></a>');
var_dump($d2->getElementsByTagName('c')->length);
var_dump($d2->documentElement->firstChild->getElementsByTagName('c')->length);
--EXPECT--
int(3)
int(3)
array(3) {
  [0]=>
  string(3) "0:1"
  [1]=>
  string(3) "1:2"
  [2]=>
  string(3) "2:3"
}
int(0)
NULL
int(1)
int(3)
int(2)
