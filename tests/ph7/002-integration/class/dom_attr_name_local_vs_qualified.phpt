--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: DOMAttr::$name is the LOCAL name while nodeName is the qualified one; Dom\Attr::$name is the qualified name
--FILE--
<?php
/* the 2004 tree hands back libxml's node name, which for an attribute carries
 * no prefix -- so `name` and `nodeName` disagree for a prefixed attribute and
 * agree for every other one */
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:u" p:b="V" c="W"/>');
$e = $d->documentElement;
foreach ($e->attributes as $k => $a) {
    echo $k, ' ', $a->name, ' ', $a->nodeName, ' ', $a->localName, "\n";
}
/* both doors onto the same attribute answer the same pair */
echo $e->getAttributeNode('p:b')->name, ' ', $e->getAttributeNode('p:b')->nodeName, "\n";
echo $e->getAttributeNodeNS('urn:u', 'b')->name, ' ', $e->getAttributeNodeNS('urn:u', 'b')->nodeName, "\n";

/* a created attribute answers by the same rule */
$a = $d->createAttributeNS('urn:x', 'q:z');
echo $a->name, ' ', $a->nodeName, "\n";
echo $d->createAttribute('plain')->name, ' ', $d->createAttribute('plain')->nodeName, "\n";

/* the modern tree answers the QUALIFIED name for both */
$x = Dom\XMLDocument::createFromString('<r xmlns:p="urn:u" p:b="V"/>');
$m = $x->documentElement->getAttributeNodeNS('urn:u', 'b');
echo $m->name, ' ', $m->nodeName, ' ', $m->localName, "\n";
$mc = $x->createAttributeNS('urn:x', 'q:z');
echo $mc->name, ' ', $mc->nodeName, "\n";
--EXPECT--
b b p:b b
c c c c
b p:b
b p:b
z q:z
plain plain
p:b p:b b
q:z q:z
