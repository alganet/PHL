--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
createElementNS builds a namespaced element with php's grammar and resolution rules
--FILE--
<?php
$dom_cens_XML   = 'http://www.w3.org/XML/1998/namespace';
$dom_cens_XMLNS = 'http://www.w3.org/2000/xmlns/';

$dom_cens_make = static function (?string $uri, string $qname, string $host = '<r/>') {
    $d = new DOMDocument;
    $d->loadXML($host);
    try { $e = $d->createElementNS($uri, $qname); }
    catch (Throwable $ex) { return get_class($ex) . '(' . $ex->getCode() . ')'; }
    if (!($e instanceof DOMNode)) return var_export($e, true);
    $fresh = $d->saveXML($e);
    $d->documentElement->appendChild($e);
    return $fresh . ' | ' . $e->nodeName . ' prefix=' . var_export($e->prefix, true)
        . ' local=' . var_export($e->localName, true) . ' ns=' . var_export($e->namespaceURI, true)
        . ' | ' . $d->saveXML($d->documentElement);
};

// The name grammar. With a namespace the name must be a QName and every failure
// is the Namespace Error; with NONE, an unprefixed name is judged as a plain
// Name -- where a character libxml will not take is the INVALID CHARACTER error
// -- and a prefixed one is refused because a prefix names a namespace.
// (A name ENDING in the colon -- `x:`, `p:` -- is left out of the no-namespace
// rows on purpose: whether libxml splits it into a prefix at all changed between
// 2.9 and 2.13, so php's own answer there is its libxml's, not a rule. With a
// namespace both versions refuse it as a QName, which is pinned below.)
foreach ([null, '', 'urn:a'] as $uri) {
    $names = ['x', 'x y', '1x', ':x', '::x', 'a:b:c', 'p:x', 'p:x y', 'p:1x', '1:x', 'x@y'];
    if ($uri !== null && $uri !== '') { $names[] = 'x:'; $names[] = 'p:'; }
    foreach ($names as $qn) {
        printf("%-8s %-8s %s\n", var_export($uri, true), '"' . $qn . '"', $dom_cens_make($uri, $qn));
    }
}

// The reserved names. php checks them where it RESOLVES the namespace, so a URI
// the document already binds is used whatever prefix was asked for -- the xml
// namespace is bound in EVERY document, which is why all four spellings of it
// come back as `xml:`.
foreach ([$dom_cens_XML, $dom_cens_XMLNS, 'urn:a'] as $uri) {
    foreach (['x', 'xml:x', 'xmlns:x', 'p:x', 'xmlns', 'xml', 'xmlns:xmlns', 'xml:xml'] as $qn) {
        printf("%-30s %-12s %s\n", $uri, '"' . $qn . '"', $dom_cens_make($uri, $qn));
    }
}

// The declaration lands on the new element, and what the document already makes
// is settled when the element is LINKED, not when it is created.
$hosts = ['<r/>', '<r xmlns:p="urn:a"/>', '<r xmlns="urn:a"/>', '<r xmlns:p="urn:a" xmlns:q="urn:a"/>'];
foreach ($hosts as $host) {
    foreach ([['urn:a', 'x'], ['urn:a', 'p:x'], ['urn:a', 'z:x'], ['urn:b', 'p:x']] as [$u, $q]) {
        printf("%-38s %-6s %s\n", $host, $q, $dom_cens_make($u, $q, $host));
    }
}

// A fresh element belongs to the document that made it and to no tree.
$d = new DOMDocument;
$e = $d->createElementNS('urn:a', 'p:x');
var_dump($e->ownerDocument === $d, $e->parentNode, $e->isConnected, $e->nodeType,
    get_class($e), $d->createElementNS('urn:a', 'p:x') === $e);

// The $value is entity-PARSED, not text -- the quirk createElement carries too.
$d = new DOMDocument;
$d->loadXML('<r/>');
$v = $d->createElementNS('urn:a', 'a', 'a &amp; b <x>');
$d->documentElement->appendChild($v);
var_dump($d->saveXML($d->documentElement), $v->textContent, $v->childNodes->length);

// Reflection sees php's signature.
$rm = new ReflectionMethod('DOMDocument', 'createElementNS');
$out = [];
foreach ($rm->getParameters() as $p) {
    $out[] = ($p->getType() ? (string)$p->getType() . ' ' : '') . '$' . $p->getName()
        . ($p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
}
var_dump(implode(', ', $out), $rm->getReturnType(), $rm->getTentativeReturnType());
?>
--EXPECT--
NULL     "x"      <x/> | x prefix='' local='x' ns=NULL | <r><x/></r>
NULL     "x y"    DOMException(5)
NULL     "1x"     DOMException(5)
NULL     ":x"     <:x/> | :x prefix='' local=':x' ns=NULL | <r><:x/></r>
NULL     "::x"    <::x/> | ::x prefix='' local='::x' ns=NULL | <r><::x/></r>
NULL     "a:b:c"  DOMException(14)
NULL     "p:x"    DOMException(14)
NULL     "p:x y"  DOMException(14)
NULL     "p:1x"   DOMException(14)
NULL     "1:x"    DOMException(14)
NULL     "x@y"    DOMException(5)
''       "x"      <x xmlns=""/> | x prefix='' local='x' ns='' | <r><x xmlns=""/></r>
''       "x y"    DOMException(5)
''       "1x"     DOMException(5)
''       ":x"     <:x xmlns=""/> | :x prefix='' local=':x' ns='' | <r><:x xmlns=""/></r>
''       "::x"    <::x xmlns=""/> | ::x prefix='' local='::x' ns='' | <r><::x xmlns=""/></r>
''       "a:b:c"  DOMException(14)
''       "p:x"    DOMException(14)
''       "p:x y"  DOMException(14)
''       "p:1x"   DOMException(14)
''       "1:x"    DOMException(14)
''       "x@y"    DOMException(5)
'urn:a'  "x"      <x xmlns="urn:a"/> | x prefix='' local='x' ns='urn:a' | <r><x xmlns="urn:a"/></r>
'urn:a'  "x y"    DOMException(14)
'urn:a'  "1x"     DOMException(14)
'urn:a'  ":x"     DOMException(14)
'urn:a'  "::x"    DOMException(14)
'urn:a'  "a:b:c"  DOMException(14)
'urn:a'  "p:x"    <p:x xmlns:p="urn:a"/> | p:x prefix='p' local='x' ns='urn:a' | <r><p:x xmlns:p="urn:a"/></r>
'urn:a'  "p:x y"  DOMException(14)
'urn:a'  "p:1x"   DOMException(14)
'urn:a'  "1:x"    DOMException(14)
'urn:a'  "x@y"    DOMException(14)
'urn:a'  "x:"     DOMException(14)
'urn:a'  "p:"     DOMException(14)
http://www.w3.org/XML/1998/namespace "x"          <xml:x/> | xml:x prefix='xml' local='x' ns='http://www.w3.org/XML/1998/namespace' | <r><xml:x/></r>
http://www.w3.org/XML/1998/namespace "xml:x"      <xml:x/> | xml:x prefix='xml' local='x' ns='http://www.w3.org/XML/1998/namespace' | <r><xml:x/></r>
http://www.w3.org/XML/1998/namespace "xmlns:x"    <xml:x/> | xml:x prefix='xml' local='x' ns='http://www.w3.org/XML/1998/namespace' | <r><xml:x/></r>
http://www.w3.org/XML/1998/namespace "p:x"        <xml:x/> | xml:x prefix='xml' local='x' ns='http://www.w3.org/XML/1998/namespace' | <r><xml:x/></r>
http://www.w3.org/XML/1998/namespace "xmlns"      <xml:xmlns/> | xml:xmlns prefix='xml' local='xmlns' ns='http://www.w3.org/XML/1998/namespace' | <r><xml:xmlns/></r>
http://www.w3.org/XML/1998/namespace "xml"        <xml:xml/> | xml:xml prefix='xml' local='xml' ns='http://www.w3.org/XML/1998/namespace' | <r><xml:xml/></r>
http://www.w3.org/XML/1998/namespace "xmlns:xmlns" <xml:xmlns/> | xml:xmlns prefix='xml' local='xmlns' ns='http://www.w3.org/XML/1998/namespace' | <r><xml:xmlns/></r>
http://www.w3.org/XML/1998/namespace "xml:xml"    <xml:xml/> | xml:xml prefix='xml' local='xml' ns='http://www.w3.org/XML/1998/namespace' | <r><xml:xml/></r>
http://www.w3.org/2000/xmlns/  "x"          <x xmlns="http://www.w3.org/2000/xmlns/"/> | x prefix='' local='x' ns='http://www.w3.org/2000/xmlns/' | <r><x xmlns="http://www.w3.org/2000/xmlns/"/></r>
http://www.w3.org/2000/xmlns/  "xml:x"      DOMException(14)
http://www.w3.org/2000/xmlns/  "xmlns:x"    <xmlns:x xmlns:xmlns="http://www.w3.org/2000/xmlns/"/> | xmlns:x prefix='xmlns' local='x' ns='http://www.w3.org/2000/xmlns/' | <r><xmlns:x xmlns:xmlns="http://www.w3.org/2000/xmlns/"/></r>
http://www.w3.org/2000/xmlns/  "p:x"        DOMException(14)
http://www.w3.org/2000/xmlns/  "xmlns"      <xmlns xmlns="http://www.w3.org/2000/xmlns/"/> | xmlns prefix='' local='xmlns' ns='http://www.w3.org/2000/xmlns/' | <r><xmlns xmlns="http://www.w3.org/2000/xmlns/"/></r>
http://www.w3.org/2000/xmlns/  "xml"        <xml xmlns="http://www.w3.org/2000/xmlns/"/> | xml prefix='' local='xml' ns='http://www.w3.org/2000/xmlns/' | <r><xml xmlns="http://www.w3.org/2000/xmlns/"/></r>
http://www.w3.org/2000/xmlns/  "xmlns:xmlns" <xmlns:xmlns xmlns:xmlns="http://www.w3.org/2000/xmlns/"/> | xmlns:xmlns prefix='xmlns' local='xmlns' ns='http://www.w3.org/2000/xmlns/' | <r><xmlns:xmlns xmlns:xmlns="http://www.w3.org/2000/xmlns/"/></r>
http://www.w3.org/2000/xmlns/  "xml:xml"    DOMException(14)
urn:a                          "x"          <x xmlns="urn:a"/> | x prefix='' local='x' ns='urn:a' | <r><x xmlns="urn:a"/></r>
urn:a                          "xml:x"      DOMException(14)
urn:a                          "xmlns:x"    DOMException(14)
urn:a                          "p:x"        <p:x xmlns:p="urn:a"/> | p:x prefix='p' local='x' ns='urn:a' | <r><p:x xmlns:p="urn:a"/></r>
urn:a                          "xmlns"      <xmlns xmlns="urn:a"/> | xmlns prefix='' local='xmlns' ns='urn:a' | <r><xmlns xmlns="urn:a"/></r>
urn:a                          "xml"        <xml xmlns="urn:a"/> | xml prefix='' local='xml' ns='urn:a' | <r><xml xmlns="urn:a"/></r>
urn:a                          "xmlns:xmlns" DOMException(14)
urn:a                          "xml:xml"    DOMException(14)
<r/>                                   x      <x xmlns="urn:a"/> | x prefix='' local='x' ns='urn:a' | <r><x xmlns="urn:a"/></r>
<r/>                                   p:x    <p:x xmlns:p="urn:a"/> | p:x prefix='p' local='x' ns='urn:a' | <r><p:x xmlns:p="urn:a"/></r>
<r/>                                   z:x    <z:x xmlns:z="urn:a"/> | z:x prefix='z' local='x' ns='urn:a' | <r><z:x xmlns:z="urn:a"/></r>
<r/>                                   p:x    <p:x xmlns:p="urn:b"/> | p:x prefix='p' local='x' ns='urn:b' | <r><p:x xmlns:p="urn:b"/></r>
<r xmlns:p="urn:a"/>                   x      <x xmlns="urn:a"/> | p:x prefix='p' local='x' ns='urn:a' | <r xmlns:p="urn:a"><p:x/></r>
<r xmlns:p="urn:a"/>                   p:x    <p:x xmlns:p="urn:a"/> | p:x prefix='p' local='x' ns='urn:a' | <r xmlns:p="urn:a"><p:x/></r>
<r xmlns:p="urn:a"/>                   z:x    <z:x xmlns:z="urn:a"/> | z:x prefix='z' local='x' ns='urn:a' | <r xmlns:p="urn:a"><z:x xmlns:z="urn:a"/></r>
<r xmlns:p="urn:a"/>                   p:x    <p:x xmlns:p="urn:b"/> | p:x prefix='p' local='x' ns='urn:b' | <r xmlns:p="urn:a"><p:x xmlns:p="urn:b"/></r>
<r xmlns="urn:a"/>                     x      <x xmlns="urn:a"/> | x prefix='' local='x' ns='urn:a' | <r xmlns="urn:a"><x/></r>
<r xmlns="urn:a"/>                     p:x    <p:x xmlns:p="urn:a"/> | p:x prefix='p' local='x' ns='urn:a' | <r xmlns="urn:a"><p:x xmlns:p="urn:a"/></r>
<r xmlns="urn:a"/>                     z:x    <z:x xmlns:z="urn:a"/> | z:x prefix='z' local='x' ns='urn:a' | <r xmlns="urn:a"><z:x xmlns:z="urn:a"/></r>
<r xmlns="urn:a"/>                     p:x    <p:x xmlns:p="urn:b"/> | p:x prefix='p' local='x' ns='urn:b' | <r xmlns="urn:a"><p:x xmlns:p="urn:b"/></r>
<r xmlns:p="urn:a" xmlns:q="urn:a"/>   x      <x xmlns="urn:a"/> | p:x prefix='p' local='x' ns='urn:a' | <r xmlns:p="urn:a" xmlns:q="urn:a"><p:x/></r>
<r xmlns:p="urn:a" xmlns:q="urn:a"/>   p:x    <p:x xmlns:p="urn:a"/> | p:x prefix='p' local='x' ns='urn:a' | <r xmlns:p="urn:a" xmlns:q="urn:a"><p:x/></r>
<r xmlns:p="urn:a" xmlns:q="urn:a"/>   z:x    <z:x xmlns:z="urn:a"/> | z:x prefix='z' local='x' ns='urn:a' | <r xmlns:p="urn:a" xmlns:q="urn:a"><z:x xmlns:z="urn:a"/></r>
<r xmlns:p="urn:a" xmlns:q="urn:a"/>   p:x    <p:x xmlns:p="urn:b"/> | p:x prefix='p' local='x' ns='urn:b' | <r xmlns:p="urn:a" xmlns:q="urn:a"><p:x xmlns:p="urn:b"/></r>
bool(true)
NULL
bool(false)
int(1)
string(10) "DOMElement"
bool(false)
string(47) "<r><a xmlns="urn:a">a &amp; b &lt;x&gt;</a></r>"
string(9) "a & b <x>"
int(1)
string(61) "?string $namespace, string $qualifiedName, string $value = ''"
NULL
NULL
