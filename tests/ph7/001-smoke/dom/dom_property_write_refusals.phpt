--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A read-only DOM property and a value of the wrong type are php's two refusals
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<a:r xmlns:a="urn:a" xmlns:b="urn:a" k="v">t<c/></a:r>');
$r = $d->documentElement;
$try = function (string $label, callable $f) {
    try { $f(); echo str_pad($label, 34), "ok\n"; }
    catch (Throwable $t) { printf("%-34s %s: %s\n", $label, get_class($t), $t->getMessage()); }
};
// A property php declares but does not let a program write is its readonly Error.
foreach (['nodeName', 'nodeType', 'tagName', 'parentNode', 'firstChild', 'childNodes',
          'attributes', 'ownerDocument', 'localName', 'namespaceURI', 'isConnected',
          'parentElement', 'childElementCount'] as $p) {
    $try("elem $p", function () use ($r, $p) { $r->$p = 'x'; });
}
$try('attr name', function () use ($r) { $r->attributes->item(0)->name = 'x'; });
$try('attr ownerElement', function () use ($r) { $r->attributes->item(0)->ownerElement = null; });
$try('text length', function () use ($r) { $r->firstChild->length = 3; });
$try('text wholeText', function () use ($r) { $r->firstChild->wholeText = 'x'; });
$try('list length', function () use ($r) { $r->childNodes->length = 3; });
$try('map length', function () use ($r) { $r->attributes->length = 3; });
$try('doc documentElement', function () use ($d) { $d->documentElement = null; });
// The declared TYPE decides what a value may be: nodeValue is ?string, the rest
// of the writable set is string, and neither takes an array or an object.
$try('nodeValue = null', function () use ($r) { $r->nodeValue = null; });
$try('textContent = null', function () use ($r) { $r->textContent = null; });
$try('prefix = null', function () use ($r) { $r->prefix = null; });
$try('className = null', function () use ($r) { $r->className = null; });
$try('nodeValue = array', function () use ($r) { $r->nodeValue = [1]; });
$try('textContent = object', function () use ($r) { $r->textContent = new stdClass; });
$try('data = array', function () use ($r) { $r->firstChild->data = []; });
$try('value = object', function () use ($r) { $r->attributes->item(0)->value = new DOMDocument; });
$try('prefix = array', function () use ($r) { $r->prefix = ['a']; });
--EXPECT--
elem nodeName                      Error: Cannot modify readonly property DOMElement::$nodeName
elem nodeType                      Error: Cannot modify readonly property DOMElement::$nodeType
elem tagName                       Error: Cannot modify readonly property DOMElement::$tagName
elem parentNode                    Error: Cannot modify readonly property DOMElement::$parentNode
elem firstChild                    Error: Cannot modify readonly property DOMElement::$firstChild
elem childNodes                    Error: Cannot modify readonly property DOMElement::$childNodes
elem attributes                    Error: Cannot modify readonly property DOMElement::$attributes
elem ownerDocument                 Error: Cannot modify readonly property DOMElement::$ownerDocument
elem localName                     Error: Cannot modify readonly property DOMElement::$localName
elem namespaceURI                  Error: Cannot modify readonly property DOMElement::$namespaceURI
elem isConnected                   Error: Cannot modify readonly property DOMElement::$isConnected
elem parentElement                 Error: Cannot modify readonly property DOMElement::$parentElement
elem childElementCount             Error: Cannot modify readonly property DOMElement::$childElementCount
attr name                          Error: Cannot modify readonly property DOMAttr::$name
attr ownerElement                  Error: Cannot modify readonly property DOMAttr::$ownerElement
text length                        Error: Cannot modify readonly property DOMText::$length
text wholeText                     Error: Cannot modify readonly property DOMText::$wholeText
list length                        Error: Cannot modify readonly property DOMNodeList::$length
map length                         Error: Cannot modify readonly property DOMNamedNodeMap::$length
doc documentElement                Error: Cannot modify readonly property DOMDocument::$documentElement
nodeValue = null                  ok
textContent = null                 TypeError: Cannot assign null to property DOMNode::$textContent of type string
prefix = null                      TypeError: Cannot assign null to property DOMNode::$prefix of type string
className = null                   TypeError: Cannot assign null to property DOMElement::$className of type string
nodeValue = array                  TypeError: Cannot assign array to property DOMNode::$nodeValue of type ?string
textContent = object               TypeError: Cannot assign stdClass to property DOMNode::$textContent of type string
data = array                       TypeError: Cannot assign array to property DOMCharacterData::$data of type string
value = object                     TypeError: Cannot assign DOMDocument to property DOMAttr::$value of type string
prefix = array                     TypeError: Cannot assign array to property DOMNode::$prefix of type string
