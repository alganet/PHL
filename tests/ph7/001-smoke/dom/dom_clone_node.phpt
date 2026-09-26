--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNode::cloneNode: shallow keeps attributes/xmlns, deep copies the subtree, a document clones into a second document
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:one" ra="1"><c k="v">t</c><!--cm--><?pi dat?><![CDATA[cd]]><p:e p:at="1"><in/></p:e></r>');
$r = $d->documentElement;

/* shallow: attributes and the element's own declarations, no children */
$s = $r->cloneNode();
var_dump(get_class($s), $s->childNodes->length, $s->attributes->length, $s->parentNode, $s->isConnected);
echo $d->saveXML($s), "\n";

/* deep: the whole subtree, still detached and still owned by the same document */
$deep = $r->cloneNode(true);
var_dump($deep->childNodes->length, $deep->ownerDocument === $d, $deep->isSameNode($r));
echo $d->saveXML($deep), "\n";

/* every clone is a distinct node */
var_dump($r->cloneNode() === $r->cloneNode());

/* a namespaced element and a namespaced attribute keep their namespace */
$ns = $r->lastChild;
echo $d->saveXML($ns->cloneNode()), "\n";
$at = $ns->attributes->item(0);
$atc = $at->cloneNode(true);
var_dump(get_class($atc), $atc->nodeName, $atc->namespaceURI, $atc->prefix, $atc->localName, $atc->value);

/* character data, comments and processing instructions */
foreach ([1, 2, 3] as $i) {
    $n = $r->childNodes->item($i);
    echo get_class($n), ' ', $d->saveXML($n->cloneNode(true)), "\n";
}

/* a clone can be spliced back in; the original is untouched */
$r->appendChild($r->firstChild->cloneNode(true));
echo $d->saveXML(), "\n";

/* the document: a SECOND document with its own tree */
$d->formatOutput = true;
$d->preserveWhiteSpace = false;
$dc = $d->cloneNode(true);
var_dump(get_class($dc), $dc === $d, $dc->preserveWhiteSpace, $dc->formatOutput);
var_dump($dc->documentElement->ownerDocument === $dc, $dc->documentElement->isSameNode($d->documentElement));
try {
    $dc->documentElement->appendChild($d->createElement('q'));
} catch (DOMException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
$shallow = $d->cloneNode();
var_dump(get_class($shallow), $shallow->documentElement);
echo $shallow->saveXML();

/* a fragment clones its children with it */
$f = $d->createDocumentFragment();
$f->appendChild($d->createElement('z'));
$f->appendChild($d->createTextNode('y'));
var_dump($f->cloneNode(true)->childNodes->length, $f->cloneNode()->childNodes->length);
--EXPECT--
string(10) "DOMElement"
int(0)
int(1)
NULL
bool(false)
<r xmlns:p="urn:one" ra="1"/>
int(5)
bool(true)
bool(false)
<r xmlns:p="urn:one" ra="1"><c k="v">t</c><!--cm--><?pi dat?><![CDATA[cd]]><p:e p:at="1"><in/></p:e></r>
bool(false)
<p:e xmlns:p="urn:one" p:at="1"/>
string(7) "DOMAttr"
string(4) "p:at"
string(7) "urn:one"
string(1) "p"
string(2) "at"
string(1) "1"
DOMComment <!--cm-->
DOMProcessingInstruction <?pi dat?>
DOMCdataSection <![CDATA[cd]]>
<?xml version="1.0"?>
<r xmlns:p="urn:one" ra="1"><c k="v">t</c><!--cm--><?pi dat?><![CDATA[cd]]><p:e p:at="1"><in/></p:e><c k="v">t</c></r>

string(11) "DOMDocument"
bool(false)
bool(false)
bool(true)
bool(true)
bool(false)
DOMException: Wrong Document Error
string(11) "DOMDocument"
NULL
<?xml version="1.0"?>
int(2)
int(0)
