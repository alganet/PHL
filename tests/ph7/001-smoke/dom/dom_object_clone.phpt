--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
clone of a DOM object copies the NODE: deep, detached, same document; a document clones whole; the collections and DOMXPath refuse
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:x"><a k="1">t<b/></a></r>');
$n = $d->documentElement->firstChild;

/* the copy is a second subtree: deep, detached, still owned by the document */
$c = clone $n;
var_dump(get_class($c), $c === $n, $c->parentNode, $c->ownerDocument === $d);
var_dump($c->childNodes->length, $c->getAttribute('k'), $c->firstChild->data);

/* a write through the copy does not show through the original */
$c->setAttribute('k', 'CHANGED');
var_dump($n->getAttribute('k'), $c->getAttribute('k'));

/* the copy's own wrappers answer the copy */
var_dump($c->firstChild === $c->firstChild, $c->firstChild->parentNode === $c);

/* a namespaced element keeps its namespace */
$d2 = new DOMDocument;
$d2->loadXML('<r xmlns:p="urn:x"><p:a/></r>');
$pc = clone $d2->documentElement->firstChild;
var_dump($pc->nodeName, $pc->namespaceURI);
echo $d2->saveXML($pc), "\n";

/* every node kind clones as its own class, detached */
$d3 = new DOMDocument;
$d3->loadXML('<!DOCTYPE r [<!ENTITY e "x">]><r><?pi d?><!--c-->&e;</r>');
foreach ($d3->documentElement->childNodes as $kid) {
    $kc = clone $kid;
    var_dump(get_class($kc), $kc->parentNode === null);
}
$at = $d3->createAttribute('w'); $at->value = 'v';
$ac = clone $at;
var_dump(get_class($ac), $ac->value, $ac->ownerElement);

/* the document: a SECOND document, directives, declaration and URI included */
$d->preserveWhiteSpace = false;
$d->formatOutput = true;
$d->strictErrorChecking = false;
$d->documentURI = '/tmp/clone-probe.xml';
$dc = clone $d;
var_dump(get_class($dc), $dc === $d, $dc->preserveWhiteSpace, $dc->formatOutput, $dc->strictErrorChecking);
var_dump($dc->documentURI, $dc->documentElement === $d->documentElement, $dc->documentElement->ownerDocument === $dc);
$dc->documentElement->setAttribute('extra', 'y');
var_dump($d->documentElement->hasAttribute('extra'), $dc->documentElement->hasAttribute('extra'));
$de = new DOMDocument('1.1', 'UTF-8');
$dec = clone $de;
echo $dec->saveXML();

/* a user subclass clones through the inherited hook, class and properties kept */
class CloneProbeDoc extends DOMDocument { public $tag = 1; }
$md = new CloneProbeDoc;
$md->loadXML('<m/>');
$md->tag = 9;
$mc = clone $md;
var_dump(get_class($mc), $mc->tag, $mc->saveXML($mc->documentElement), $mc->documentElement === $md->documentElement);

/* php's uncloneable three, and a subclass is refused with ITS name */
foreach ([$d->documentElement->childNodes, $d->documentElement->firstChild->attributes, new DOMXPath($d)] as $o) {
    try { clone $o; echo "cloned\n"; }
    catch (Error $e) { echo $e->getMessage(), "\n"; }
}
class CloneProbeXPath extends DOMXPath {}
try { clone new CloneProbeXPath($d); }
catch (Error $e) { echo $e->getMessage(), "\n"; }

/* a DOMException rides Exception's refusal */
try { clone new DOMException('m'); }
catch (Error $e) { echo $e->getMessage(), "\n"; }

/* Reflection agrees, subclasses included */
foreach (['DOMNode', 'DOMDocument', 'DOMNodeList', 'DOMNamedNodeMap', 'DOMXPath', 'CloneProbeXPath'] as $cn) {
    var_dump($cn, (new ReflectionClass($cn))->isCloneable());
}

/* php declares NO __clone on these classes: the copy is a handler, not a method */
var_dump(method_exists($d, '__clone'), method_exists($n, '__clone'));
--EXPECT--
string(10) "DOMElement"
bool(false)
NULL
bool(true)
int(2)
string(1) "1"
string(1) "t"
string(1) "1"
string(7) "CHANGED"
bool(true)
bool(true)
string(3) "p:a"
string(5) "urn:x"
<p:a xmlns:p="urn:x"/>
string(24) "DOMProcessingInstruction"
bool(true)
string(10) "DOMComment"
bool(true)
string(18) "DOMEntityReference"
bool(true)
string(7) "DOMAttr"
string(1) "v"
NULL
string(11) "DOMDocument"
bool(false)
bool(false)
bool(true)
bool(false)
string(20) "/tmp/clone-probe.xml"
bool(false)
bool(true)
bool(false)
bool(true)
<?xml version="1.1" encoding="UTF-8"?>
string(13) "CloneProbeDoc"
int(9)
string(4) "<m/>"
bool(false)
Trying to clone an uncloneable object of class DOMNodeList
Trying to clone an uncloneable object of class DOMNamedNodeMap
Trying to clone an uncloneable object of class DOMXPath
Trying to clone an uncloneable object of class CloneProbeXPath
Trying to clone an uncloneable object of class DOMException
string(7) "DOMNode"
bool(true)
string(11) "DOMDocument"
bool(true)
string(11) "DOMNodeList"
bool(false)
string(15) "DOMNamedNodeMap"
bool(false)
string(8) "DOMXPath"
bool(false)
string(15) "CloneProbeXPath"
bool(false)
bool(false)
bool(false)
