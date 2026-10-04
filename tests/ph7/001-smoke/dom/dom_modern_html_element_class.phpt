--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An element in the HTML namespace wears its own class in php's namespaced tree
--FILE--
<?php
// php 8.4's tree has one class whose name is not a function of the node TYPE:
// an element carrying the HTML namespace is a Dom\HTMLElement and any other
// element is a Dom\Element. The question is about the node's namespace alone --
// the document it sits in is never asked, so an xhtml root parsed into a
// Dom\XMLDocument is an HTML element too, and a no-namespace element is not.
// The 2004 tree has no such class and keeps answering DOMElement throughout.
const H = 'http://www.w3.org/1999/xhtml';

$x = Dom\XMLDocument::createEmpty();
$e = $x->createElementNS(H, 'p');
echo 'create: ', get_class($e), "\n";
echo 'clone: ', get_class($e->cloneNode(true)), "\n";
echo 'prefixed: ', get_class($x->createElementNS(H, 'h:z')), "\n";
$x->appendChild($e);
echo 'documentElement: ', get_class($x->documentElement), "\n";
echo 'identity kept: ', var_export($x->documentElement === $e, true), "\n";

// The same three answers off a parse, where the wrapper is minted by a walk and
// not by the producer.
$p = Dom\XMLDocument::createFromString(
    '<r xmlns="' . H . '"><b/><c xmlns="urn:z"/><d xmlns=""/></r>'
);
echo 'parsed root: ', get_class($p->documentElement), "\n";
foreach ($p->documentElement->childNodes as $n) {
    echo '  child ', $n->nodeName, ': ', get_class($n), "\n";
}

// An attribute in the HTML namespace is still an attribute, and the 2004 tree
// is untouched by any of it.
echo 'attr: ', get_class($p->createAttributeNS(H, 'a:b')), "\n";
$o = new DOMDocument();
$o->loadXML('<r xmlns="' . H . '"><b/></r>');
echo 'old root: ', get_class($o->documentElement), "\n";
echo 'old child: ', get_class($o->documentElement->firstChild), "\n";

// registerNodeClass keys on the class php would have handed out, so the two
// element classes are registered separately and neither catches the other's.
class MyEl extends Dom\Element {}
class MyHtml extends Dom\HTMLElement {}

$r = Dom\XMLDocument::createEmpty();
$r->registerNodeClass(Dom\Element::class, MyEl::class);
echo 'Element registered, html: ', get_class($r->createElementNS(H, 'p')), "\n";
echo 'Element registered, other: ', get_class($r->createElementNS('urn:z', 'p')), "\n";

$r2 = Dom\XMLDocument::createEmpty();
$r2->registerNodeClass(Dom\HTMLElement::class, MyHtml::class);
echo 'HTMLElement registered, html: ', get_class($r2->createElementNS(H, 'p')), "\n";
echo 'HTMLElement registered, other: ', get_class($r2->createElementNS('urn:z', 'p')), "\n";
--EXPECT--
create: Dom\HTMLElement
clone: Dom\HTMLElement
prefixed: Dom\HTMLElement
documentElement: Dom\HTMLElement
identity kept: true
parsed root: Dom\HTMLElement
  child b: Dom\HTMLElement
  child c: Dom\Element
  child d: Dom\Element
attr: Dom\Attr
old root: DOMElement
old child: DOMElement
Element registered, html: Dom\HTMLElement
Element registered, other: MyEl
HTMLElement registered, html: MyHtml
HTMLElement registered, other: Dom\Element
