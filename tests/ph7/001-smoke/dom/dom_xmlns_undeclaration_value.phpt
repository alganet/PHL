--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNameSpaceNode nodeValue of an xmlns="" undeclaration is null (namespaceURI stays "")
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r xmlns="urn:d"><a><b xmlns=""><c/></b></a></r>');
$b = $d->getElementsByTagName('b')->item(0);
// the attribute door: an xmlns="" UNDECLARATION answers null nodeValue, "" URI
$n = $b->getAttributeNode('xmlns');
var_dump(get_class($n), $n->nodeValue, $n->namespaceURI, $n->nodeName, $n->prefix, $n->localName);
var_dump($b->getAttribute('xmlns'), $b->hasAttribute('xmlns'));
// ...and a real declaration still answers its URI through both
$r = $d->documentElement;
$nr = $r->getAttributeNode('xmlns');
var_dump($nr->nodeValue, $nr->namespaceURI);
--EXPECT--
string(16) "DOMNameSpaceNode"
NULL
string(0) ""
string(5) "xmlns"
string(0) ""
string(5) "xmlns"
string(0) ""
bool(true)
string(5) "urn:d"
string(5) "urn:d"
