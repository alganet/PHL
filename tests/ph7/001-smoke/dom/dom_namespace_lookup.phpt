--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNode::lookupNamespaceURI/lookupPrefix/isDefaultNamespace
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<a:r xmlns:a="urn:a" xmlns="urn:def">'
    . '<c a:k="v">t</c><d:e xmlns:d="urn:d"/></a:r>');
$r = $d->documentElement;
$c = $r->firstChild;
$e = $r->lastChild;
$t = $c->firstChild;
$k = $c->attributes->item(0);
$lone = $d->createElement('lone');
$empty = new DOMDocument;
foreach (['doc' => $d, 'root' => $r, 'c' => $c, 'text' => $t, 'attr' => $k,
          'e' => $e, 'detached' => $lone, 'emptydoc' => $empty] as $label => $n) {
    printf("%-9s uri(a)=%-8s uri(d)=%-8s uri(null)=%-10s prefix(urn:a)=%-4s prefix(urn:def)=%-4s default(urn:def)=%s default('')=%s\n",
        $label,
        var_export($n->lookupNamespaceURI('a'), true),
        var_export($n->lookupNamespaceURI('d'), true),
        var_export($n->lookupNamespaceURI(null), true),
        var_export($n->lookupPrefix('urn:a'), true),
        var_export($n->lookupPrefix('urn:def'), true),
        var_export($n->isDefaultNamespace('urn:def'), true),
        var_export($n->isDefaultNamespace(''), true));
}
// The reserved xml prefix is always in scope, and an unknown URI has no prefix.
var_dump($r->lookupNamespaceURI('xml'), $r->lookupPrefix('urn:nope'), $r->lookupNamespaceURI(''));
--EXPECT--
doc       uri(a)='urn:a'  uri(d)=NULL     uri(null)='urn:def'  prefix(urn:a)='a'  prefix(urn:def)=NULL default(urn:def)=true default('')=false
root      uri(a)='urn:a'  uri(d)=NULL     uri(null)='urn:def'  prefix(urn:a)='a'  prefix(urn:def)=NULL default(urn:def)=true default('')=false
c         uri(a)='urn:a'  uri(d)=NULL     uri(null)='urn:def'  prefix(urn:a)='a'  prefix(urn:def)=NULL default(urn:def)=true default('')=false
text      uri(a)='urn:a'  uri(d)=NULL     uri(null)='urn:def'  prefix(urn:a)='a'  prefix(urn:def)=NULL default(urn:def)=true default('')=false
attr      uri(a)='urn:a'  uri(d)=NULL     uri(null)='urn:def'  prefix(urn:a)='a'  prefix(urn:def)=NULL default(urn:def)=true default('')=false
e         uri(a)='urn:a'  uri(d)='urn:d'  uri(null)='urn:def'  prefix(urn:a)='a'  prefix(urn:def)=NULL default(urn:def)=true default('')=false
detached  uri(a)=NULL     uri(d)=NULL     uri(null)=NULL       prefix(urn:a)=NULL prefix(urn:def)=NULL default(urn:def)=false default('')=false
emptydoc  uri(a)=NULL     uri(d)=NULL     uri(null)=NULL       prefix(urn:a)=NULL prefix(urn:def)=NULL default(urn:def)=false default('')=false
string(36) "http://www.w3.org/XML/1998/namespace"
NULL
NULL
