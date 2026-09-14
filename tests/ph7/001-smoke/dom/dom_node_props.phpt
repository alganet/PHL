--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNode property surface: nodeName/nodeValue/nodeType/textContent/childNodes
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r a="v">t1<k>t2</k><!--c--><![CDATA[cd]]></r>');
$r = $d->documentElement;
echo $d->nodeName, '|', $r->nodeName, '|', $r->tagName, "\n";
var_dump($d->nodeType, $r->nodeType);
var_dump($d->nodeValue, $r->nodeValue);
var_dump($r->textContent, $d->textContent);
var_dump($r->childElementCount);
foreach ($r->childNodes as $i => $c) {
    echo $i, ':', $c->nodeName, ':', $c->nodeType, ':', var_export($c->nodeValue, true), "\n";
}
$k = $r->childNodes->item(1);
var_dump($k->firstChild->nodeName, $k->lastChild->nodeValue);
var_dump($r->firstChild->nextSibling === $k, $k->previousSibling->nodeName);
var_dump($r->ownerDocument === $d, $d->ownerDocument);
$t = $r->firstChild;
var_dump($t instanceof DOMText, $t->data, $t->length);
$cm = $r->childNodes->item(2);
var_dump($cm instanceof DOMComment, $cm->data);
$cd = $r->childNodes->item(3);
var_dump($cd instanceof DOMCdataSection, $cd instanceof DOMText, $cd->data);
var_dump($r->attributes->item(0)->nodeName);
--EXPECT--
#document|r|r
int(9)
int(1)
NULL
string(6) "t1t2cd"
string(6) "t1t2cd"
string(6) "t1t2cd"
int(1)
0:#text:3:'t1'
1:k:1:'t2'
2:#comment:8:'c'
3:#cdata-section:4:'cd'
string(5) "#text"
string(2) "t2"
bool(true)
string(5) "#text"
bool(true)
NULL
bool(true)
string(2) "t1"
int(2)
bool(true)
string(1) "c"
bool(true)
bool(true)
string(2) "cd"
string(1) "a"
