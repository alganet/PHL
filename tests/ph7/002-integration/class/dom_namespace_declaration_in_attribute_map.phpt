--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: php 8.4's attribute map answers a namespace declaration where the 2004 map lists none
--FILE--
<?php
/* php 8.4's tree puts a namespace DECLARATION in the element's attribute map:
 * `attributes` lists it as an attribute of its own AHEAD of the attributes,
 * `length` counts it, and every by-name door on the map finds it. The 2004 tree
 * lists none and answers a declaration through DOMNameSpaceNode instead. The
 * wrapper is attribute-shaped either way it is reached, and it is the SAME
 * object each time, so a write to it goes to the declaration. */
$xml = '<r xmlns:p="urn:p" xmlns="urn:d" a="1" p:b="2"/>';
$NS = 'http://www.w3.org/2000/xmlns/';
$show = static function ($n) {
    if ($n === null) { return 'null'; }
    if ($n === false) { return 'false'; }
    return sprintf('%s(%d) %s|%s|%s|%s=%s', get_class($n), $n->nodeType, $n->nodeName,
        $n->localName, $n->prefix ?? '-', $n->namespaceURI ?? '-', $n->nodeValue);
};
foreach (['modern' => Dom\XMLDocument::createFromString($xml),
          '  2004' => (static function () use ($xml) { $d = new DOMDocument; $d->loadXML($xml); return $d; })()] as $tree => $doc) {
    $e = $doc->documentElement;
    $m = $e->attributes;
    echo "== $tree: length ", $m->length, "\n";
    foreach ($m as $k => $a) {
        echo "  [", $k, "] ", $show($a), "\n";
    }
    foreach (['xmlns:p', 'xmlns', 'a', 'xmlns:zz'] as $n) {
        echo "  getNamedItem(", $n, ") ", $show($m->getNamedItem($n)), "\n";
    }
    echo "  getNamedItemNS(NS,p)     ", $show($m->getNamedItemNS($NS, 'p')), "\n";
    echo "  getNamedItemNS(NS,xmlns) ", $show($m->getNamedItemNS($NS, 'xmlns')), "\n";
    echo "  getNamedItemNS(null,xmlns:p) ", $show($m->getNamedItemNS(null, 'xmlns:p')), "\n";
    echo "  getNamedItemNS(null,p)   ", $show($m->getNamedItemNS(null, 'p')), "\n";
    echo "  \$m['xmlns'] ", $show($m['xmlns'] ?? null), "\n";
    echo "  getAttributeNode(xmlns:p)     ", $show($e->getAttributeNode('xmlns:p')), "\n";
    echo "  getAttributeNodeNS(NS,p)      ", $show($e->getAttributeNodeNS($NS, 'p')), "\n";
    echo "  getAttributeNodeNS(NS,xmlns)  ", $show($e->getAttributeNodeNS($NS, 'xmlns')), "\n";
    /* one declaration, one wrapper: the identity every other node has. Only
     * php 8.4's tree answers one here at all -- the 2004 map has no door to a
     * declaration, and its element answers a DOMNameSpaceNode, which carries
     * no children and no ownerElement to ask about. */
    $one = $m->getNamedItem('xmlns:p');
    if ($one === null) {
        continue;
    }
    echo "  same object twice ", var_export($one === $m->getNamedItem('xmlns:p'), true), "\n";
    echo "  child ", get_class($one->firstChild), '(', $one->firstChild->data, ") of ", $one->childNodes->length, "\n";
    echo "  ownerElement ", $one->ownerElement->nodeName, " ownerDocument ", get_class($one->ownerDocument), "\n";
    /* the stand-in is a view of the declaration, not a copy of it */
    $one->value = 'urn:rewritten';
    echo "  after write: ", $e->getAttribute('xmlns:p'), " / ", $show($m->getNamedItem('xmlns:p')), "\n";
}
/* an element that declares nothing counts only its attributes, and one that
 * declares only a default namespace still answers a map of one */
echo "no decl ", Dom\XMLDocument::createFromString('<r a="1"/>')->documentElement->attributes->length, "\n";
$only = Dom\XMLDocument::createFromString('<r xmlns="urn:d"/>')->documentElement->attributes;
echo "only default ", $only->length, " ", $show($only->item(0)), "\n";
echo "past the end ", $show($only->item(1)), "\n";
/* Only a declaration the document SPELLS is in the map. A binding the engine
 * had to invent to name what it was asked to make -- createElementNS's prefix,
 * setAttributeNS's -- serializes exactly like one and is listed nowhere; the
 * `xmlns` attribute door writes a spelt one. */
$d = Dom\XMLDocument::createFromString('<r/>');
$e = $d->documentElement;
$e->setAttributeNS('urn:p', 'p:z', '2');
$c = $d->createElementNS('urn:q', 'q:c');
$e->appendChild($c);
$k = $d->createElement('k');
$e->appendChild($k);
$k->setAttributeNS($NS, 'xmlns:w', 'urn:w');
foreach (['setAttributeNS bound' => $e, 'createElementNS bound' => $c, 'xmlns written' => $k] as $why => $n) {
    echo $why, ' ', $n->attributes->length, ' ';
    foreach ($n->attributes as $a) { echo $a->nodeName, ' '; }
    echo "\n";
}
echo $d->C14N(), "\n";
?>
--EXPECT--
== modern: length 4
  [xmlns:p] Dom\Attr(2) xmlns:p|p|xmlns|http://www.w3.org/2000/xmlns/=urn:p
  [xmlns] Dom\Attr(2) xmlns|xmlns|-|http://www.w3.org/2000/xmlns/=urn:d
  [a] Dom\Attr(2) a|a|-|-=1
  [p:b] Dom\Attr(2) p:b|b|p|urn:p=2
  getNamedItem(xmlns:p) Dom\Attr(2) xmlns:p|p|xmlns|http://www.w3.org/2000/xmlns/=urn:p
  getNamedItem(xmlns) Dom\Attr(2) xmlns|xmlns|-|http://www.w3.org/2000/xmlns/=urn:d
  getNamedItem(a) Dom\Attr(2) a|a|-|-=1
  getNamedItem(xmlns:zz) null
  getNamedItemNS(NS,p)     Dom\Attr(2) xmlns:p|p|xmlns|http://www.w3.org/2000/xmlns/=urn:p
  getNamedItemNS(NS,xmlns) Dom\Attr(2) xmlns|xmlns|-|http://www.w3.org/2000/xmlns/=urn:d
  getNamedItemNS(null,xmlns:p) Dom\Attr(2) xmlns:p|p|xmlns|http://www.w3.org/2000/xmlns/=urn:p
  getNamedItemNS(null,p)   null
  $m['xmlns'] Dom\Attr(2) xmlns|xmlns|-|http://www.w3.org/2000/xmlns/=urn:d
  getAttributeNode(xmlns:p)     Dom\Attr(2) xmlns:p|p|xmlns|http://www.w3.org/2000/xmlns/=urn:p
  getAttributeNodeNS(NS,p)      Dom\Attr(2) xmlns:p|p|xmlns|http://www.w3.org/2000/xmlns/=urn:p
  getAttributeNodeNS(NS,xmlns)  Dom\Attr(2) xmlns|xmlns|-|http://www.w3.org/2000/xmlns/=urn:d
  same object twice true
  child Dom\Text(urn:p) of 1
  ownerElement r ownerDocument Dom\XMLDocument
  after write: urn:rewritten / Dom\Attr(2) xmlns:p|p|xmlns|http://www.w3.org/2000/xmlns/=urn:rewritten
==   2004: length 2
  [a] DOMAttr(2) a|a||-=1
  [b] DOMAttr(2) p:b|b|p|urn:p=2
  getNamedItem(xmlns:p) null
  getNamedItem(xmlns) null
  getNamedItem(a) DOMAttr(2) a|a||-=1
  getNamedItem(xmlns:zz) null
  getNamedItemNS(NS,p)     null
  getNamedItemNS(NS,xmlns) null
  getNamedItemNS(null,xmlns:p) null
  getNamedItemNS(null,p)   null
  $m['xmlns'] null
  getAttributeNode(xmlns:p)     DOMNameSpaceNode(18) xmlns:p|p|p|urn:p=urn:p
  getAttributeNodeNS(NS,p)      DOMNameSpaceNode(18) xmlns:p|p|p|urn:p=urn:p
  getAttributeNodeNS(NS,xmlns)  null
no decl 1
only default 1 Dom\Attr(2) xmlns|xmlns|-|http://www.w3.org/2000/xmlns/=urn:d
past the end null
setAttributeNS bound 1 p:z 
createElementNS bound 0 
xmlns written 1 xmlns:w 
<r xmlns:p="urn:p" p:z="2"><q:c xmlns:q="urn:q"></q:c><k xmlns:w="urn:w"></k></r>
