--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespace an HTML parse implies is on no attribute surface, and writing one puts it there
--FILE--
<?php
/* The HTML tree construction rules namespace an element whatever the source
 * wrote: an `<html>` root is XHTML's, an `<svg>` subtree SVG's, a `<math>` one
 * MathML's, and an `xlink:href` inside foreign content carries the xlink
 * namespace with no `xmlns:xlink` written anywhere.  None of those bindings is
 * an attribute -- the document spelled none of them -- so every door onto the
 * attribute surface answers as it does for an element with no attributes. */
$dom_ins_doc = function ($x) { return Dom\HTMLDocument::createFromString($x, LIBXML_NOERROR); };
$dom_ins_show = function ($l, $e) {
  echo "== $l\n";
  echo "  namespaceURI: ", var_export($e->namespaceURI, true), "\n";
  echo "  length: ", $e->attributes->length, " count: ", count(iterator_to_array($e->attributes)), "\n";
  echo "  names: ", json_encode($e->getAttributeNames()), "\n";
  echo "  hasAttributes: ", var_export($e->hasAttributes(), true), "\n";
  echo "  hasAttribute(xmlns): ", var_export($e->hasAttribute('xmlns'), true), "\n";
  echo "  getAttribute(xmlns): ", var_export($e->getAttribute('xmlns'), true), "\n";
  echo "  getAttributeNode(xmlns): ", var_export($e->getAttributeNode('xmlns'), true), "\n";
  echo "  getNamedItem(xmlns): ", var_export($e->attributes->getNamedItem('xmlns'), true), "\n";
  echo "  item(0): ", var_export($e->attributes->item(0)?->name, true), "\n";
};

$dom_ins_d = $dom_ins_doc('<!DOCTYPE html><html><body><svg><use xlink:href="#a"/></svg><math><mi>x</mi></math></body></html>');
$dom_ins_show('the html root', $dom_ins_d->documentElement);
$dom_ins_show('an svg root', $dom_ins_d->getElementsByTagName('svg')[0]);
$dom_ins_show('the use that carries an xlink name', $dom_ins_d->getElementsByTagName('use')[0]);
$dom_ins_show('a math root', $dom_ins_d->getElementsByTagName('math')[0]);

/* The xlink name itself IS namespaced, and the element's own namespace is
 * answered by the readers that ask the scope rather than the map. */
$dom_ins_u = $dom_ins_d->getElementsByTagName('use')[0];
echo "== the bindings themselves\n";
echo "  attr namespaceURI: ", var_export($dom_ins_u->attributes[0]->namespaceURI, true), "\n";
echo "  attr prefix: ", var_export($dom_ins_u->attributes[0]->prefix, true), "\n";
echo "  hasAttributeNS(xmlns, xmlns): ", var_export($dom_ins_u->hasAttributeNS('http://www.w3.org/2000/xmlns/', 'xmlns'), true), "\n";
echo "  getAttributeNS(xmlns, xmlns): ", var_export($dom_ins_u->getAttributeNS('http://www.w3.org/2000/xmlns/', 'xmlns'), true), "\n";

/* A copy carries the same answer: nothing is spelled on either side of it. */
$dom_ins_c = $dom_ins_d->documentElement->cloneNode(true);
$dom_ins_show('a deep clone of the root', $dom_ins_c);
$dom_ins_x = Dom\XMLDocument::createEmpty();
$dom_ins_i = $dom_ins_x->importNode($dom_ins_d->getElementsByTagName('svg')[0], true);
$dom_ins_show('the svg imported into an XML document', $dom_ins_i);

/* ...and writing one SPELLS it, so the same doors answer from then on. */
$dom_ins_w = $dom_ins_doc('<html><body></body></html>')->documentElement;
$dom_ins_w->setAttributeNS('http://www.w3.org/2000/xmlns/', 'xmlns:p', 'urn:p');
$dom_ins_show('after a written xmlns:p', $dom_ins_w);
echo "  hasAttribute(xmlns:p): ", var_export($dom_ins_w->hasAttribute('xmlns:p'), true), "\n";
echo "  getAttribute(xmlns:p): ", var_export($dom_ins_w->getAttribute('xmlns:p'), true), "\n";
echo "  removeAttribute(xmlns:p): ", var_export($dom_ins_w->removeAttribute('xmlns:p'), true), "\n";
echo "  names after the remove: ", json_encode($dom_ins_w->getAttributeNames()), "\n";
?>
--EXPECT--
== the html root
  namespaceURI: 'http://www.w3.org/1999/xhtml'
  length: 0 count: 0
  names: []
  hasAttributes: false
  hasAttribute(xmlns): false
  getAttribute(xmlns): NULL
  getAttributeNode(xmlns): NULL
  getNamedItem(xmlns): NULL
  item(0): NULL
== an svg root
  namespaceURI: 'http://www.w3.org/2000/svg'
  length: 0 count: 0
  names: []
  hasAttributes: false
  hasAttribute(xmlns): false
  getAttribute(xmlns): NULL
  getAttributeNode(xmlns): NULL
  getNamedItem(xmlns): NULL
  item(0): NULL
== the use that carries an xlink name
  namespaceURI: 'http://www.w3.org/2000/svg'
  length: 1 count: 1
  names: ["xlink:href"]
  hasAttributes: true
  hasAttribute(xmlns): false
  getAttribute(xmlns): NULL
  getAttributeNode(xmlns): NULL
  getNamedItem(xmlns): NULL
  item(0): 'xlink:href'
== a math root
  namespaceURI: 'http://www.w3.org/1998/Math/MathML'
  length: 0 count: 0
  names: []
  hasAttributes: false
  hasAttribute(xmlns): false
  getAttribute(xmlns): NULL
  getAttributeNode(xmlns): NULL
  getNamedItem(xmlns): NULL
  item(0): NULL
== the bindings themselves
  attr namespaceURI: 'http://www.w3.org/1999/xlink'
  attr prefix: 'xlink'
  hasAttributeNS(xmlns, xmlns): false
  getAttributeNS(xmlns, xmlns): NULL
== a deep clone of the root
  namespaceURI: 'http://www.w3.org/1999/xhtml'
  length: 0 count: 0
  names: []
  hasAttributes: false
  hasAttribute(xmlns): false
  getAttribute(xmlns): NULL
  getAttributeNode(xmlns): NULL
  getNamedItem(xmlns): NULL
  item(0): NULL
== the svg imported into an XML document
  namespaceURI: 'http://www.w3.org/2000/svg'
  length: 0 count: 0
  names: []
  hasAttributes: false
  hasAttribute(xmlns): false
  getAttribute(xmlns): NULL
  getAttributeNode(xmlns): NULL
  getNamedItem(xmlns): NULL
  item(0): NULL
== after a written xmlns:p
  namespaceURI: 'http://www.w3.org/1999/xhtml'
  length: 1 count: 1
  names: ["xmlns:p"]
  hasAttributes: true
  hasAttribute(xmlns): false
  getAttribute(xmlns): NULL
  getAttributeNode(xmlns): NULL
  getNamedItem(xmlns): NULL
  item(0): 'xmlns:p'
  hasAttribute(xmlns:p): true
  getAttribute(xmlns:p): 'urn:p'
  removeAttribute(xmlns:p): NULL
  names after the remove: []
