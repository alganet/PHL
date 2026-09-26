--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMXPath namespace:: axis: DOMNameSpaceNode results, parented on the element the axis ran on
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:p" xmlns:q="urn:q"><b xmlns:z="urn:z" xmlns:p="urn:p2"/></r>');
$xp = new DOMXPath($d);
// the axis answers DOMNameSpaceNode, parented on the element the axis ran ON
$res = $xp->query('//b/namespace::*');
var_dump($res->length);
foreach ($res as $n) {
  echo get_class($n), ' ', $n->nodeName, ' local=', $n->localName, ' prefix=', $n->prefix,
    ' uri=', $n->namespaceURI, ' value=', $n->nodeValue, ' type=', $n->nodeType,
    ' parent=', $n->parentNode->nodeName, ' doc=', ($n->ownerDocument === $d ? 'same' : 'OTHER'), "\n";
}
// inherited declarations answer through the descendant too (shadowing applies)
$r2 = $xp->query('namespace::*', $d->documentElement);
var_dump($r2->length);
foreach ($r2 as $n) { echo $n->nodeName, '=', $n->nodeValue, ' '; }
echo "\n";
// a document with no declarations still answers the implicit xml one
$d3 = new DOMDocument;
$d3->loadXML('<only/>');
$xp3 = new DOMXPath($d3);
$r3 = $xp3->query('namespace::*', $d3->documentElement);
var_dump($r3->length);
foreach ($r3 as $n) { echo $n->nodeName, '=', $n->nodeValue, "\n"; }
// one query's item is one object; two queries are two (equal, not identical)
$l = $xp->query('//b/namespace::*');
var_dump($l->item(0) === $l->item(0));
$a1 = $xp->query('//b/namespace::p')->item(0);
$a2 = $xp->query('//b/namespace::p')->item(0);
var_dump($a1 === $a2, $a1 == $a2, $a1->nodeValue);
// evaluate() wraps the same way, and count() sees the pseudo-nodes
$e = $xp->evaluate('//b/namespace::*');
var_dump(get_class($e), $e->length);
var_dump($xp->evaluate('count(//b/namespace::*)'));
// a union mixes them with ordinary nodes
$d4 = new DOMDocument;
$d4->loadXML('<r xmlns:p="urn:p" k="v"/>');
$xp4 = new DOMXPath($d4);
$u = $xp4->query('//r/@*|//r/namespace::*');
var_dump($u->length);
foreach ($u as $n) { echo get_class($n), ':', $n->nodeName, ' '; }
echo "\n";
// item() past the end, and the axis on an element with an ancestor chain
var_dump($u->item(5));
$d5 = new DOMDocument;
$d5->loadXML('<a xmlns:o="urn:o"><b><c/></b></a>');
$xp5 = new DOMXPath($d5);
$c = $d5->getElementsByTagName('c')->item(0);
$r5 = $xp5->query('namespace::o', $c);
var_dump($r5->length, $r5->item(0)->parentNode->nodeName, $r5->item(0)->nodeValue);
--EXPECT--
int(4)
DOMNameSpaceNode xmlns:xml local=xml prefix=xml uri=http://www.w3.org/XML/1998/namespace value=http://www.w3.org/XML/1998/namespace type=18 parent=b doc=same
DOMNameSpaceNode xmlns:q local=q prefix=q uri=urn:q value=urn:q type=18 parent=b doc=same
DOMNameSpaceNode xmlns:p local=p prefix=p uri=urn:p2 value=urn:p2 type=18 parent=b doc=same
DOMNameSpaceNode xmlns:z local=z prefix=z uri=urn:z value=urn:z type=18 parent=b doc=same
int(3)
xmlns:xml=http://www.w3.org/XML/1998/namespace xmlns:q=urn:q xmlns:p=urn:p 
int(1)
xmlns:xml=http://www.w3.org/XML/1998/namespace
bool(true)
bool(false)
bool(true)
string(6) "urn:p2"
string(11) "DOMNodeList"
int(4)
float(4)
int(3)
DOMAttr:k DOMNameSpaceNode:xmlns:xml DOMNameSpaceNode:xmlns:p 
NULL
int(1)
string(1) "c"
string(5) "urn:o"
