--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNode isEqualNode / contains / getRootNode / compareDocumentPosition + the six DOCUMENT_POSITION constants / isSupported
--FILE--
<?php
$k = (new ReflectionClass('DOMNode'))->getConstants();
ksort($k);
echo json_encode($k), "\n";
/* every node class inherits them */
var_dump(DOMElement::DOCUMENT_POSITION_CONTAINED_BY, DOMText::DOCUMENT_POSITION_PRECEDING);

$d = new DOMDocument;
$d->loadXML('<r a="1" b="2"><m x="1">t</m><m x="1">t</m><m x="2">t</m><k/>tx<!--c--><?pi d?></r>');
$r = $d->documentElement;
$c = $r->childNodes;

/* isEqualNode: structure, not identity */
var_dump($c->item(0)->isEqualNode($c->item(1)), $c->item(0)->isEqualNode($c->item(2)));
var_dump($c->item(0)->isEqualNode($c->item(0)), $c->item(0)->isEqualNode(null));
var_dump($c->item(0)->isEqualNode($c->item(3)));
$d2 = new DOMDocument;
$d2->loadXML('<r><m x="1">t</m></r>');
var_dump($c->item(0)->isEqualNode($d2->documentElement->firstChild));
var_dump($d->isEqualNode($d2), $d->isEqualNode($d->cloneNode(true)));
var_dump($c->item(4)->isEqualNode($d->createTextNode('tx')),
         $c->item(4)->isEqualNode($d->createTextNode('ty')),
         $c->item(4)->isEqualNode($d->createCDATASection('tx')));
var_dump($c->item(6)->isEqualNode($d->createProcessingInstruction('pi', 'd')),
         $c->item(6)->isEqualNode($d->createProcessingInstruction('pi', 'e')),
         $c->item(6)->isEqualNode($d->createProcessingInstruction('po', 'd')));
var_dump($d->createEntityReference('zz')->isEqualNode($d->createEntityReference('zz')),
         $d->createEntityReference('zz')->isEqualNode($d->createEntityReference('yy')));

/* attributes compare as a SET; an element's PREFIX counts, an attribute's does not */
$o = new DOMDocument;
$o->loadXML('<r xmlns:p="urn:x" xmlns:q="urn:x"><m a="1" b="2"/><m b="2" a="1"/><m a="1"/>'
    . '<p:e/><q:e/><e xmlns="urn:x"/><m p:a="1"/><m q:a="1"/></r>');
$m = $o->documentElement->childNodes;
var_dump($m->item(0)->isEqualNode($m->item(1)), $m->item(0)->isEqualNode($m->item(2)));
var_dump($m->item(3)->isEqualNode($m->item(4)), $m->item(3)->isEqualNode($m->item(5)));
var_dump($m->item(6)->isEqualNode($m->item(7)), $m->item(6)->isEqualNode($m->item(2)));
var_dump($m->item(6)->attributes->item(0)->isEqualNode($m->item(7)->attributes->item(0)));

/* contains: INCLUSIVE descendant, and an element contains its own attributes */
var_dump($r->contains($c->item(0)), $r->contains($r), $c->item(0)->contains($r));
var_dump($r->contains(null), $r->contains($c->item(0)->firstChild), $d->contains($r));
var_dump($r->contains($d2->documentElement));
var_dump($c->item(0)->contains($c->item(0)->attributes->item(0)));

/* getRootNode */
var_dump($r->getRootNode() === $d, $r->getRootNode()->nodeName);
$orphan = $d->createElement('o');
$orphan->appendChild($d->createElement('i'));
var_dump($orphan->firstChild->getRootNode()->nodeName);
var_dump($r->attributes->item(0)->getRootNode()->nodeName, $r->getRootNode(['composed' => true])->nodeName);

/* compareDocumentPosition */
var_dump($r->compareDocumentPosition($c->item(0)), $c->item(0)->compareDocumentPosition($r));
var_dump($c->item(0)->compareDocumentPosition($c->item(1)), $c->item(1)->compareDocumentPosition($c->item(0)));
var_dump($r->compareDocumentPosition($r), $d->compareDocumentPosition($r));
$a0 = $r->attributes->item(0);
$a1 = $r->attributes->item(1);
var_dump($a0->compareDocumentPosition($a1), $a1->compareDocumentPosition($a0));
var_dump($r->compareDocumentPosition($a0), $a0->compareDocumentPosition($r));
$mx = $c->item(0)->attributes->item(0);
var_dump($a0->compareDocumentPosition($mx), $mx->compareDocumentPosition($a0));
var_dump($mx->compareDocumentPosition($c->item(1)), $c->item(1)->compareDocumentPosition($mx));
/* disconnected: only the DISCONNECTED|IMPLEMENTATION_SPECIFIC pair is portable --
 * php picks the direction from the raw node pointers and says so in the bit. */
$far = $r->compareDocumentPosition($d2->documentElement);
$back = $d2->documentElement->compareDocumentPosition($r);
var_dump($far & 33, $back & 33, ($far & 6) !== ($back & 6), ($far & 6) === 2 || ($far & 6) === 4);

/* isSupported: two rows, feature folds case, version does not */
foreach ([['XML','1.0'],['XML','2.0'],['Core','1.0'],['Core','2.0'],['xml','1.0'],
          ['CORE','1.0'],['HTML','1.0'],['',''],['XML','3.0'],['XML','1']] as $p) {
    var_dump($r->isSupported($p[0], $p[1]));
}
--EXPECT--
{"DOCUMENT_POSITION_CONTAINED_BY":16,"DOCUMENT_POSITION_CONTAINS":8,"DOCUMENT_POSITION_DISCONNECTED":1,"DOCUMENT_POSITION_FOLLOWING":4,"DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC":32,"DOCUMENT_POSITION_PRECEDING":2}
int(16)
int(2)
bool(true)
bool(false)
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
string(9) "#document"
string(1) "o"
string(9) "#document"
string(9) "#document"
int(20)
int(10)
int(4)
int(2)
int(0)
int(20)
int(36)
int(34)
int(20)
int(10)
int(4)
int(2)
int(4)
int(2)
int(33)
int(33)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
