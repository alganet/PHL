--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: the by-name attribute lookup resolves a prefix on the 2004 tree and matches it literally on php 8.4's
--FILE--
<?php
/* two prefixes bound to ONE uri: the 2004 rule resolves the sought prefix and
 * matches by (local name, uri), so `q:b` finds `p:b`; php 8.4's matches the
 * qualified name as WRITTEN, so it does not. The map asks a third question on
 * the old tree -- libxml's stored name, which for a prefixed attribute is the
 * local part alone. */
$xml = '<r xmlns:p="urn:x" xmlns:q="urn:x"><e p:b="1" c="2" xml:lang="en"/></r>';
$d = new DOMDocument;
$d->loadXML($xml);
$old = $d->documentElement->firstElementChild;
$new = Dom\XMLDocument::createFromString($xml)->documentElement->firstElementChild;
/* the node a door found, by the name it wears -- the legacy getAttributeNode
 * answers `false` and the modern one `null`, so neither is printed as a bool */
$hit = static fn($n) => $n ? $n->nodeName : '-';
foreach (['p:b', 'q:b', 'b', 'c', 'xml:lang', 'lang', 'z:b'] as $q) {
    printf("%-9s old %d %-8s %-8s  new %d %-8s %s\n", $q,
        $old->hasAttribute($q),
        $hit($old->getAttributeNode($q)),
        $hit($old->attributes->getNamedItem($q)),
        $new->hasAttribute($q),
        $hit($new->getAttributeNode($q)),
        $hit($new->attributes->getNamedItem($q)));
}
/* the foreach key and the subscript are that same lookup, one per tree */
foreach ($old->attributes as $k => $v) { echo "old key [$k]\n"; }
foreach ($new->attributes as $k => $v) { echo "new key [$k]\n"; }
var_dump($old->attributes['p:b'], $new->attributes['p:b']->value);
/* getNamedItemNS' null namespace is the map's ANY, so it asks the same one */
var_dump($new->attributes->getNamedItemNS(null, 'p:b')->value,
         $new->attributes->getNamedItemNS(null, 'b'));
/* removeAttribute and toggleAttribute come through the same door: an
 * unresolvable spelling removes nothing on the modern tree */
$new->removeAttribute('q:b');
echo $new->getAttribute('p:b'), "\n";
$new->removeAttribute('p:b');
var_dump($new->getAttribute('p:b'));
?>
--EXPECT--
p:b       old 1 p:b      -         new 1 p:b      p:b
q:b       old 1 p:b      -         new 0 -        -
b         old 0 -        p:b       new 0 -        -
c         old 1 c        c         new 1 c        c
xml:lang  old 1 xml:lang -         new 1 xml:lang xml:lang
lang      old 0 -        xml:lang  new 0 -        -
z:b       old 0 -        -         new 0 -        -
old key [b]
old key [c]
old key [lang]
new key [p:b]
new key [c]
new key [xml:lang]
NULL
string(1) "1"
string(1) "1"
NULL
1
NULL
