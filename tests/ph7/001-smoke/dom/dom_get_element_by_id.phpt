--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getElementById answers only what a DTD or setIdAttribute made an ID, and only in the tree
--FILE--
<?php
$at = static function ($x) {
    if ($x instanceof DOMNode) return get_class($x) . '/' . $x->getNodePath();
    return var_export($x, true);
};

// An `id` attribute is NOT an ID: nothing says so in an XML document.
$doc = new DOMDocument;
$doc->loadXML('<r><c id="a"/><d ref="a"/></r>');
$kid = $doc->documentElement->firstChild;
var_dump($at($doc->getElementById('a')), $kid->getAttributeNode('id')->isId());

// setIdAttribute is what says so, and getElementById is what it says it FOR.
$kid->setIdAttribute('id', true);
var_dump($at($doc->getElementById('a')), $doc->getElementById('a') === $kid);

// The table follows the VALUE, and the flag can be taken back.
$kid->setAttribute('id', 'b');
var_dump($at($doc->getElementById('a')), $at($doc->getElementById('b')));
$kid->setIdAttribute('id', false);
var_dump($at($doc->getElementById('b')));
$kid->setIdAttributeNode($kid->getAttributeNode('id'), true);
var_dump($at($doc->getElementById('b')));
$kid->removeAttribute('id');
var_dump($at($doc->getElementById('b')));

// A DTD that declares one needs nothing else, and a duplicate answers the first.
$dtd = new DOMDocument;
$dtd->loadXML('<?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r ANY><!ELEMENT c ANY>'
    . '<!ATTLIST c id ID #IMPLIED>]><r><c id="i1"/><c id="i2"/></r>');
var_dump($at($dtd->getElementById('i1')), $at($dtd->getElementById('i2')),
    $at($dtd->getElementById('zz')), $at($dtd->getElementById('')),
    $dtd->documentElement->firstChild->getAttributeNode('id')->isId());

$dup = new DOMDocument;
$dup->loadXML('<r><a id="d"/><b id="d"/></r>');
$dup->documentElement->firstChild->setIdAttribute('id', true);
$dup->documentElement->lastChild->setIdAttribute('id', true);
var_dump($at($dup->getElementById('d')));

// An element taken OUT of the tree stops being findable, flag and all.
$moved = new DOMDocument;
$moved->loadXML('<r><c id="m"/></r>');
$gone = $moved->documentElement->firstChild;
$gone->setIdAttribute('id', true);
var_dump($at($moved->getElementById('m')));
$moved->documentElement->removeChild($gone);
var_dump($at($moved->getElementById('m')), $gone->getAttributeNode('id')->isId());
$moved->documentElement->appendChild($gone);
var_dump($at($moved->getElementById('m')));

// A namespaced ID attribute is marked through the NS spelling.
$ns = new DOMDocument;
$ns->loadXML('<r xmlns:x="urn:x"><c x:id="q"/></r>');
$ns->documentElement->firstChild->setIdAttributeNS('urn:x', 'id', true);
var_dump($at($ns->getElementById('q')));
--EXPECT--
string(4) "NULL"
bool(false)
string(15) "DOMElement//r/c"
bool(true)
string(4) "NULL"
string(15) "DOMElement//r/c"
string(4) "NULL"
string(15) "DOMElement//r/c"
string(4) "NULL"
string(18) "DOMElement//r/c[1]"
string(18) "DOMElement//r/c[2]"
string(4) "NULL"
string(4) "NULL"
bool(true)
string(15) "DOMElement//r/a"
string(15) "DOMElement//r/c"
string(4) "NULL"
bool(true)
string(15) "DOMElement//r/c"
string(15) "DOMElement//r/c"
