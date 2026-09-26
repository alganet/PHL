--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset()/empty() on a DOM virtual property, and the Undefined property warning
--FILE--
<?php
set_error_handler(function ($no, $str) {
    if (!(error_reporting() & $no)) { return false; }
    echo "handler: $str\n";
    return true;
});
$d = new DOMDocument;
$d->loadXML('<r a="1">t<k/></r>');
$r = $d->documentElement;
$t = $r->firstChild;
// isset() reads php's virtual property and answers on its VALUE: a name that
// exists but reads null is not set.
foreach (['nodeName', 'nodeValue', 'firstChild', 'nextSibling', 'attributes',
          'localName', 'namespaceURI', 'isConnected', 'parentElement', 'nosuch'] as $p) {
    printf("%-13s isset=%d\n", $p, (int) isset($r->$p));
}
var_dump(isset($d->documentElement), isset($t->wholeText), isset($t->data));
var_dump(isset($r->childNodes->length), isset($r->attributes->length));
var_dump(isset($r->childNodes->nope), isset($r->attributes->nope));
// empty() is the same read, and `??` skips the warning a plain read raises.
var_dump(empty($r->nodeName), empty($r->nextSibling), $r->nosuch ?? 'fallback');
echo "--- reads\n";
var_dump($r->nosuch);
var_dump($d->nosuch);
var_dump($t->nosuch);
var_dump($r->attributes->item(0)->nosuch);
var_dump($r->childNodes->nosuch);
var_dump($r->attributes->nosuch);
--EXPECT--
nodeName      isset=1
nodeValue     isset=1
firstChild    isset=1
nextSibling   isset=0
attributes    isset=1
localName     isset=1
namespaceURI  isset=0
isConnected   isset=1
parentElement isset=0
nosuch        isset=0
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
string(8) "fallback"
--- reads
handler: Undefined property: DOMElement::$nosuch
NULL
handler: Undefined property: DOMDocument::$nosuch
NULL
handler: Undefined property: DOMText::$nosuch
NULL
handler: Undefined property: DOMAttr::$nosuch
NULL
handler: Undefined property: DOMNodeList::$nosuch
NULL
handler: Undefined property: DOMNamedNodeMap::$nosuch
NULL
