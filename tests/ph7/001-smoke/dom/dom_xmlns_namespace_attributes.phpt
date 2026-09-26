--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The xmlns namespace is answered through both its doors: declarations and real attributes
--FILE--
<?php
$dom_xa_X = 'http://www.w3.org/2000/xmlns/';

// The xmlns namespace is reached through TWO doors, and php answers about
// either: a DECLARATION, which is not an attribute in libxml at all, and a real
// attribute IN that namespace, which is what createAttributeNS makes.
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:p" xmlns="urn:d"><c/></r>');
$r = $d->documentElement;
foreach (['p', 'xmlns', 'q'] as $ln) {
    printf("decl %-6s has=%d get=%s\n", $ln, (int)$r->hasAttributeNS($dom_xa_X, $ln),
        var_export($r->getAttributeNS($dom_xa_X, $ln), true));
}
// A DEFAULT declaration is NOT one of them: php answers false and "" for the
// local name `xmlns` however the document declares one.
var_dump($r->hasAttributeNS($dom_xa_X, 'xmlns'), $r->getAttributeNS($dom_xa_X, 'xmlns'));

// The unprefixed `xmlns` attribute -- the only name the create-side grammar lets
// through with that namespace. It IS in the namespace, and nothing is DECLARED
// for it.
$d2 = new DOMDocument;
$d2->loadXML('<r/>');
$a = $d2->createAttributeNS($dom_xa_X, 'xmlns');
printf("made: name=%s prefix=%s local=%s ns=%s | doc=%s\n", $a->nodeName,
    var_export($a->prefix, true), var_export($a->localName, true),
    var_export($a->namespaceURI, true), $d2->saveXML($d2->documentElement));

// Written onto an element it is a default-namespace declaration in the bytes,
// stays an attribute in the xmlns namespace to every question, and acquires no
// declaration of the xmlns namespace itself.
$a->value = 'urn:z';
$ret = $d2->documentElement->setAttributeNodeNS($a);
printf("set: doc=%s | name=%s ns=%s ret=%s\n", $d2->saveXML($d2->documentElement),
    $a->nodeName, var_export($a->namespaceURI, true), $ret === null ? 'null' : get_class($ret));
printf("  has=%d getNS=%s getByName=%s elemNs=%s lookup=%s\n",
    (int)$d2->documentElement->hasAttributeNS($dom_xa_X, 'xmlns'),
    var_export($d2->documentElement->getAttributeNS($dom_xa_X, 'xmlns'), true),
    var_export($d2->documentElement->getAttribute('xmlns'), true),
    var_export($d2->documentElement->namespaceURI, true),
    var_export($d2->documentElement->lookupNamespaceURI(null), true));
$rr = new DOMDocument;
printf("  reparse=%s\n", @$rr->loadXML($d2->saveXML()) ? 'ok' : 'FAILED');

// The prefixed sibling is an attribute too, not a working declaration.
$d3 = new DOMDocument;
$d3->loadXML('<r/>');
$b = $d3->createAttributeNS($dom_xa_X, 'xmlns:b');
$b->value = 'urn:b';
$d3->documentElement->setAttributeNodeNS($b);
printf("prefixed: doc=%s | name=%s ns=%s has=%d lookup=%s\n", $d3->saveXML($d3->documentElement),
    $b->nodeName, var_export($b->namespaceURI, true),
    (int)$d3->documentElement->hasAttributeNS($dom_xa_X, 'b'),
    var_export($d3->documentElement->lookupNamespaceURI('b'), true));

// A document that already binds the xmlns namespace under a prefix: the created
// attribute REUSES that binding rather than carrying its own.
$d4 = new DOMDocument;
$d4->loadXML('<r/>');
$d4->documentElement->setAttributeNodeNS($d4->createAttributeNS($dom_xa_X, 'xmlns:k'));
$a4 = $d4->createAttributeNS($dom_xa_X, 'xmlns');
printf("reuse: %s prefix=%s ns=%s | doc=%s\n", $a4->nodeName, var_export($a4->prefix, true),
    var_export($a4->namespaceURI, true), $d4->saveXML($d4->documentElement));
?>
--EXPECT--
decl p      has=1 get='urn:p'
decl xmlns  has=0 get=''
decl q      has=0 get=''
bool(false)
string(0) ""
made: name=xmlns prefix='' local='xmlns' ns='http://www.w3.org/2000/xmlns/' | doc=<r/>
set: doc=<r xmlns="urn:z"/> | name=xmlns ns='http://www.w3.org/2000/xmlns/' ret=null
  has=1 getNS='urn:z' getByName='' elemNs=NULL lookup=NULL
  reparse=ok
prefixed: doc=<r xmlns:xmlns="http://www.w3.org/2000/xmlns/" xmlns:b="urn:b"/> | name=xmlns:b ns='http://www.w3.org/2000/xmlns/' has=1 lookup=NULL
reuse: xmlns:xmlns prefix='xmlns' ns='http://www.w3.org/2000/xmlns/' | doc=<r xmlns:xmlns="http://www.w3.org/2000/xmlns/" xmlns:k=""/>
