--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An xmlns NAME is a declaration on the 2004 write doors and an attribute on the modern ones
--FILE--
<?php
// An `xmlns` NAME is read as a namespace declaration by some attribute doors
// and as an ordinary attribute by others, and which is which is php's 2004
// tree against php 8.4's. The 2004 doors read the name; the modern ones do
// not, so there `setAttribute` and `toggleAttribute` agree with each other
// and the element declares nothing.
$dom_xw_mk = function (string $kind): array {
    switch ($kind) {
        case '2004-xml':
            $d = new DOMDocument;
            $d->loadXML('<r/>');
            return [$d, $d->documentElement];
        case '2004-html':
            $d = new DOMDocument;
            @$d->loadHTML('<html><body><p>x</p></body></html>', LIBXML_NOERROR);
            return [$d, $d->getElementsByTagName('p')->item(0)];
        case 'modern-xml':
            $d = Dom\XMLDocument::createFromString('<r/>');
            return [$d, $d->documentElement];
        default:
            $d = Dom\HTMLDocument::createFromString('<html><body><p>x</p></body></html>', LIBXML_NOERROR);
            return [$d, $d->getElementsByTagName('p')->item(0)];
    }
};
$dom_xw_show = function ($d, $e, string $label): void {
    $names = [];
    foreach ($e->attributes as $a) {
        $names[] = $a->nodeName . '{' . var_export($a->namespaceURI, true) . '}=' . $a->value;
    }
    printf("%-38s attrs=[%s] ser=%s\n", $label, implode(' ', $names),
        $d instanceof Dom\HTMLDocument ? $d->saveHtml($e) : $d->saveXML($e));
};

// A declaration is invisible on the 2004 attribute map and IS an attribute on
// the modern one, so the map alone says which was written.
foreach (['2004-xml', '2004-html', 'modern-xml', 'modern-html'] as $dom_xw_k) {
    foreach (['xmlns', 'xmlns:foo'] as $dom_xw_n) {
        [$dom_xw_d, $dom_xw_e] = $dom_xw_mk($dom_xw_k);
        $dom_xw_e->setAttribute($dom_xw_n, 'urn:z');
        $dom_xw_show($dom_xw_d, $dom_xw_e, "$dom_xw_k set($dom_xw_n)");
        [$dom_xw_d, $dom_xw_e] = $dom_xw_mk($dom_xw_k);
        $dom_xw_e->toggleAttribute($dom_xw_n);
        $dom_xw_show($dom_xw_d, $dom_xw_e, "$dom_xw_k toggle($dom_xw_n)");
    }
}

// The 2004 write answers a plain true rather than the attribute node every
// other name gets, because there is no attribute node to hand back...
$dom_xw_d = new DOMDocument;
$dom_xw_d->loadXML('<r><c/></r>');
$dom_xw_r = $dom_xw_d->documentElement;
var_dump($dom_xw_r->setAttribute('xmlns', 'urn:z'));
var_dump(get_debug_type($dom_xw_r->setAttribute('xmlns:foo', 'urn:y')));
// ...the element does not MOVE into what it declares, and neither does a child
// appended after the write...
$dom_xw_r->appendChild($dom_xw_d->createElement('late'));
var_dump($dom_xw_r->namespaceURI, $dom_xw_d->getElementsByTagName('late')->item(0)->namespaceURI);
// ...and the declaration is what the by-name readers answer about.
var_dump($dom_xw_r->hasAttribute('xmlns'), $dom_xw_r->getAttribute('xmlns'));
echo $dom_xw_d->saveXML();

// A second write is dropped: php will not write THROUGH a declaration the
// element already makes, whichever value it carries.
var_dump($dom_xw_r->setAttribute('xmlns', 'urn:w'));
echo $dom_xw_d->saveXML($dom_xw_r), "\n";
var_dump($dom_xw_r->removeAttribute('xmlns'));
echo $dom_xw_d->saveXML($dom_xw_r), "\n";

// An empty value is a real binding to the empty URI, not an absent one.
$dom_xw_d2 = new DOMDocument;
$dom_xw_d2->loadXML('<r/>');
$dom_xw_d2->documentElement->setAttribute('xmlns', '');
echo $dom_xw_d2->saveXML($dom_xw_d2->documentElement), "\n";
var_dump($dom_xw_d2->documentElement->hasAttribute('xmlns'));
--EXPECT--
2004-xml set(xmlns)                    attrs=[] ser=<r xmlns="urn:z"/>
2004-xml toggle(xmlns)                 attrs=[] ser=<r xmlns=""/>
2004-xml set(xmlns:foo)                attrs=[xmlns:foo{NULL}=urn:z] ser=<r xmlns:foo="urn:z"/>
2004-xml toggle(xmlns:foo)             attrs=[] ser=<r xmlns:foo=""/>
2004-html set(xmlns)                   attrs=[] ser=<p xmlns="urn:z">x</p>
2004-html toggle(xmlns)                attrs=[] ser=<p xmlns="">x</p>
2004-html set(xmlns:foo)               attrs=[xmlns:foo{NULL}=urn:z] ser=<p xmlns:foo="urn:z">x</p>
2004-html toggle(xmlns:foo)            attrs=[] ser=<p xmlns:foo="">x</p>
modern-xml set(xmlns)                  attrs=[xmlns{NULL}=urn:z] ser=<r xmlns="urn:z"/>
modern-xml toggle(xmlns)               attrs=[xmlns{NULL}=] ser=<r xmlns=""/>
modern-xml set(xmlns:foo)              attrs=[xmlns:foo{NULL}=urn:z] ser=<r xmlns:foo="urn:z"/>
modern-xml toggle(xmlns:foo)           attrs=[xmlns:foo{NULL}=] ser=<r xmlns:foo=""/>
modern-html set(xmlns)                 attrs=[xmlns{NULL}=urn:z] ser=<p xmlns="urn:z">x</p>
modern-html toggle(xmlns)              attrs=[xmlns{NULL}=] ser=<p xmlns="">x</p>
modern-html set(xmlns:foo)             attrs=[xmlns:foo{NULL}=urn:z] ser=<p xmlns:foo="urn:z">x</p>
modern-html toggle(xmlns:foo)          attrs=[xmlns:foo{NULL}=] ser=<p xmlns:foo="">x</p>
bool(true)
string(7) "DOMAttr"
NULL
NULL
bool(true)
string(5) "urn:z"
<?xml version="1.0"?>
<r xmlns="urn:z" xmlns:foo="urn:y"><c/><late/></r>
bool(false)
<r xmlns="urn:z" xmlns:foo="urn:y"><c/><late/></r>
bool(true)
<r xmlns:foo="urn:y"><c/><late/></r>
<r xmlns=""/>
bool(true)
