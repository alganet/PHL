--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNode namespace properties: namespaceURI/prefix/localName/isConnected/parentElement
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<a:r xmlns:a="urn:a" xmlns="urn:def" a:k="v" plain="p">'
    . '<c/><d:e xmlns:d="urn:d"/>t<!--cm--><?pi dat?></a:r>');
$r = $d->documentElement;
$kids = $r->childNodes;
$rows = [
    'document' => $d,
    'root'     => $r,
    'default'  => $kids->item(0),
    'prefixed' => $kids->item(1),
    'text'     => $kids->item(2),
    'comment'  => $kids->item(3),
    'pi'       => $kids->item(4),
    'ns-attr'  => $r->attributes->item(0),
    'attr'     => $r->attributes->item(1),
    'detached' => $d->createElement('lone'),
];
foreach ($rows as $label => $n) {
    printf("%-9s type=%2d ns=%-8s prefix=%-4s local=%-6s connected=%s parentEl=%s\n",
        $label, $n->nodeType,
        var_export($n->namespaceURI, true), var_export($n->prefix, true),
        var_export($n->localName, true), var_export($n->isConnected, true),
        $n->parentElement === null ? 'null' : $n->parentElement->nodeName);
}
// The root element's parent is the document, so parentElement stops there while
// parentNode does not; an attribute reports the element it hangs off both ways.
var_dump($r->parentNode instanceof DOMDocument, $r->parentElement);
var_dump($r->attributes->item(1)->parentElement === $r);
--EXPECT--
document  type= 9 ns=NULL     prefix=''   local=NULL   connected=true parentEl=null
root      type= 1 ns='urn:a'  prefix='a'  local='r'    connected=true parentEl=null
default   type= 1 ns='urn:def' prefix=''   local='c'    connected=true parentEl=a:r
prefixed  type= 1 ns='urn:d'  prefix='d'  local='e'    connected=true parentEl=a:r
text      type= 3 ns=NULL     prefix=''   local=NULL   connected=true parentEl=a:r
comment   type= 8 ns=NULL     prefix=''   local=NULL   connected=true parentEl=a:r
pi        type= 7 ns=NULL     prefix=''   local=NULL   connected=true parentEl=a:r
ns-attr   type= 2 ns='urn:a'  prefix='a'  local='k'    connected=true parentEl=a:r
attr      type= 2 ns=NULL     prefix=''   local='plain' connected=true parentEl=a:r
detached  type= 1 ns=NULL     prefix=''   local='lone' connected=false parentEl=null
bool(true)
NULL
bool(true)
