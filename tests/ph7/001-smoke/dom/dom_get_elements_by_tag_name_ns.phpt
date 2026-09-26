--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getElementsByTagNameNS finds elements by namespace with php's asymmetric wildcards
--FILE--
<?php
$dom_gns_xml = '<r xmlns="urn:d" xmlns:p="urn:p"><a/><p:a/><b xmlns=""><a/></b><p:b><a/><p:a/></p:b></r>';
$dom_gns_d = new DOMDocument;
$dom_gns_d->loadXML($dom_gns_xml);
$dom_gns_show = static function (DOMNodeList $l): string {
    $o = [];
    foreach ($l as $n) $o[] = $n->nodeName . '{' . ($n->namespaceURI ?? 'NULL') . '}';
    return $l->length . ' [' . implode(' ', $o) . ']';
};

// The three namespace cases are NOT symmetric: `*` is every element in a
// namespace AND in none, a null or empty argument is only the elements in NO
// namespace, and a URI is only the elements in it. The local name takes `*` for
// every name and matches the LOCAL name otherwise, so a prefix never enters in.
foreach ([['urn:d', 'a'], ['urn:p', 'a'], ['*', 'a'], [null, 'a'], ['', 'a'],
          ['urn:d', '*'], ['urn:p', '*'], ['*', '*'], [null, '*'], ['', '*'],
          ['urn:none', 'a'], ['urn:d', ''], ['*', ''], ['*', 'zzz']] as [$ns, $ln]) {
    printf("doc(%-10s,%-4s) %s\n", var_export($ns, true), var_export($ln, true),
        $dom_gns_show($dom_gns_d->getElementsByTagNameNS($ns, $ln)));
}

// Rooted at an ELEMENT the receiver itself is never in the list.
$dom_gns_pb = $dom_gns_d->getElementsByTagNameNS('urn:p', 'b')->item(0);
foreach ([['*', '*'], [null, 'a'], ['urn:p', 'a'], ['urn:d', 'a']] as [$ns, $ln]) {
    printf("elem(%-10s,%-4s) %s\n", var_export($ns, true), var_export($ln, true),
        $dom_gns_show($dom_gns_pb->getElementsByTagNameNS($ns, $ln)));
}
var_dump($dom_gns_show($dom_gns_d->documentElement->getElementsByTagNameNS('*', 'r')));

// item() past the end and below zero, and the class of the answer.
$dom_gns_l = $dom_gns_d->getElementsByTagNameNS('urn:d', 'a');
var_dump(get_class($dom_gns_l), $dom_gns_l->item(99), $dom_gns_l->item(-1),
    $dom_gns_l instanceof Countable, count($dom_gns_l));

// The list is LIVE -- it re-walks the tree, it does not hold a snapshot.
var_dump($dom_gns_l->length);
$dom_gns_d->documentElement->appendChild($dom_gns_d->createElementNS('urn:d', 'a'));
var_dump($dom_gns_l->length);
$dom_gns_d->documentElement->removeChild($dom_gns_d->documentElement->lastChild);
var_dump($dom_gns_l->length);

// A document with no root answers an empty list rather than failing.
$dom_gns_e = new DOMDocument;
var_dump($dom_gns_e->getElementsByTagNameNS('*', '*')->length);

// Reflection sees php's signature on both classes.
foreach (['DOMDocument', 'DOMElement'] as $c) {
    $rm = new ReflectionMethod($c, 'getElementsByTagNameNS');
    $ps = [];
    foreach ($rm->getParameters() as $p) {
        $ps[] = ($p->getType() ? (string)$p->getType() . ' ' : '') . '$' . $p->getName();
    }
    printf("%s::%s(%s) tentative=%s\n", $c, $rm->getName(), implode(', ', $ps),
        var_export($rm->getTentativeReturnType() ? (string)$rm->getTentativeReturnType() : null, true));
}
?>
--EXPECT--
doc('urn:d'   ,'a' ) 2 [a{urn:d} a{urn:d}]
doc('urn:p'   ,'a' ) 2 [p:a{urn:p} p:a{urn:p}]
doc('*'       ,'a' ) 5 [a{urn:d} p:a{urn:p} a{NULL} a{urn:d} p:a{urn:p}]
doc(NULL      ,'a' ) 1 [a{NULL}]
doc(''        ,'a' ) 1 [a{NULL}]
doc('urn:d'   ,'*' ) 3 [r{urn:d} a{urn:d} a{urn:d}]
doc('urn:p'   ,'*' ) 3 [p:a{urn:p} p:b{urn:p} p:a{urn:p}]
doc('*'       ,'*' ) 8 [r{urn:d} a{urn:d} p:a{urn:p} b{NULL} a{NULL} p:b{urn:p} a{urn:d} p:a{urn:p}]
doc(NULL      ,'*' ) 2 [b{NULL} a{NULL}]
doc(''        ,'*' ) 2 [b{NULL} a{NULL}]
doc('urn:none','a' ) 0 []
doc('urn:d'   ,''  ) 0 []
doc('*'       ,''  ) 0 []
doc('*'       ,'zzz') 0 []
elem('*'       ,'*' ) 2 [a{urn:d} p:a{urn:p}]
elem(NULL      ,'a' ) 0 []
elem('urn:p'   ,'a' ) 1 [p:a{urn:p}]
elem('urn:d'   ,'a' ) 1 [a{urn:d}]
string(4) "0 []"
string(11) "DOMNodeList"
NULL
NULL
bool(true)
int(2)
int(2)
int(3)
int(2)
int(0)
DOMDocument::getElementsByTagNameNS(?string $namespace, string $localName) tentative='DOMNodeList'
DOMElement::getElementsByTagNameNS(?string $namespace, string $localName) tentative='DOMNodeList'
