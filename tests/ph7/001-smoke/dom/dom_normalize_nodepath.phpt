--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNode::normalize merges adjacent text and DROPS the empty ones (normalizeDocument too); DOMNode::getNodePath
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:one"><a b="1"/><p:q p:z="2"/><!--cm--><?pi d?><![CDATA[cd]]></r>');
$r = $d->documentElement;
$a = $r->firstChild;

$a->appendChild($d->createTextNode('x'));
$keep = $a->lastChild;
$a->appendChild($d->createTextNode(''));
$gone = $a->lastChild;
$a->appendChild($d->createTextNode('y'));
$tail = $a->lastChild;
$r->appendChild($d->createTextNode(''));
$empty = $r->lastChild;

echo $d->saveXML($r), "\n";
var_dump($r->childNodes->length, $a->childNodes->length);
var_dump($r->normalize());
echo $d->saveXML($r), "\n";
var_dump($r->childNodes->length, $a->childNodes->length);

/* the merged-away and dropped nodes stay readable and become parentless */
var_dump($keep->data, $keep->parentNode->nodeName);
var_dump($gone->data, $gone->parentNode, $tail->data, $tail->parentNode);
var_dump($empty->data, $empty->parentNode);

/* a child element's ATTRIBUTES are normalized too; the receiver's own are not */
$q = $r->childNodes->item(1);
$z = $q->attributes->item(0);
$z->appendChild($d->createTextNode('Z'));
$z->appendChild($d->createTextNode(''));
var_dump($z->childNodes->length);
$q->normalize();
var_dump($z->childNodes->length);
$r->normalize();
var_dump($z->childNodes->length, $z->value);

/* a CDATA section is not a text node: it neither merges nor is dropped */
$c = new DOMDocument;
$c->loadXML('<k/>');
$c->documentElement->appendChild($c->createCDATASection(''));
$c->documentElement->appendChild($c->createTextNode(''));
$c->normalizeDocument();
var_dump($c->documentElement->childNodes->length);
echo $c->saveXML($c->documentElement), "\n";

/* getNodePath */
$p = new DOMDocument;
$p->loadXML('<r xmlns:p="urn:one"><a/><a b="1"/><p:a/>t<!--c--><?pi ?><![CDATA[q]]></r>');
var_dump($p->getNodePath(), $p->documentElement->getNodePath());
foreach ($p->documentElement->childNodes as $n) {
    echo get_class($n), ' ', var_export($n->getNodePath(), true), "\n";
}
var_dump($p->documentElement->childNodes->item(1)->attributes->item(0)->getNodePath());
$orphan = $p->createElement('o');
$orphan->appendChild($p->createElement('i'));
var_dump($orphan->getNodePath(), $orphan->firstChild->getNodePath());
$frag = $p->createDocumentFragment();
$frag->appendChild($p->createElement('u'));
var_dump($frag->getNodePath(), $frag->firstChild->getNodePath());
--EXPECT--
<r xmlns:p="urn:one"><a b="1">xy</a><p:q p:z="2"/><!--cm--><?pi d?><![CDATA[cd]]></r>
int(6)
int(3)
NULL
<r xmlns:p="urn:one"><a b="1">xy</a><p:q p:z="2"/><!--cm--><?pi d?><![CDATA[cd]]></r>
int(5)
int(1)
string(2) "xy"
string(1) "a"
string(0) ""
NULL
string(1) "y"
NULL
string(0) ""
NULL
int(3)
int(3)
int(1)
string(2) "2Z"
int(1)
<k><![CDATA[]]></k>
string(1) "/"
string(2) "/r"
DOMElement '/r/a[1]'
DOMElement '/r/a[2]'
DOMElement '/r/p:a'
DOMText '/r/text()[1]'
DOMComment '/r/comment()'
DOMProcessingInstruction '/r/processing-instruction(\'pi\')'
DOMCdataSection '/r/text()[2]'
string(10) "/r/a[2]/@b"
string(2) "/o"
string(4) "/o/i"
NULL
NULL
