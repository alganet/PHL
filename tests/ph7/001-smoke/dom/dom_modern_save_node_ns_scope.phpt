--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A node saved on its own out of the namespaced tree declares the bindings its ancestors held
--FILE--
<?php
function dom_snns_at(string $xml, string $xpath)
{
    $d = Dom\XMLDocument::createFromString($xml);
    $x = new Dom\XPath($d);
    $x->registerNamespace('P', 'urn:p');
    $x->registerNamespace('Q', 'urn:q');
    return [$d, $x->query($xpath)->item(0)];
}

function dom_snns_new(string $label, string $xml, string $xpath)
{
    [$d, $n] = dom_snns_at($xml, $xpath);
    printf("%-30s %s\n", $label, $n === null ? '(no node)' : $d->saveXml($n));
}

function dom_snns_old(string $label, string $xml, string $xpath)
{
    $d = new DOMDocument;
    $d->loadXML($xml);
    $x = new DOMXPath($d);
    $x->registerNamespace('P', 'urn:p');
    $x->registerNamespace('Q', 'urn:q');
    $n = $x->query($xpath)->item(0);
    printf("%-30s %s\n", $label, $n === null ? '(no node)' : $d->saveXML($n));
}

/* The binding a node names lives on an ancestor the dump does not reach, so the
 * bytes have to carry it or they re-parse into no namespace at all. */
$dom_snns_rows = [
    ['prefixed, anc binding',      '<r xmlns:p="urn:p"><p:z/></r>',                                 '//P:z'],
    ['default ns element',         '<r xmlns="urn:p"><z/></r>',                                     '//P:z'],
    ['no-ns elem under default',   '<r xmlns="urn:p"><P:q xmlns:P="urn:q"><z xmlns=""/></P:q></r>', '//z'],
    ['elem+attr same uri',         '<r xmlns:p="urn:p"><p:z p:k="1"/></r>',                         '//P:z'],
    ['inner shadows prefix',       '<r xmlns:p="urn:p"><p:z><p:w xmlns:p="urn:q"/></p:z></r>',      '//P:z'],
    ['default + prefixed both',    '<r xmlns="urn:p" xmlns:q="urn:q"><z q:k="1"/></r>',             '//P:z'],
    ['elem ns unused deeper',      '<r xmlns:p="urn:p" xmlns:q="urn:q"><p:z><q:w/></p:z></r>',      '//P:z'],
    ['anc default, elem prefixed', '<r xmlns="urn:q" xmlns:p="urn:p"><p:z><w/></p:z></r>',          '//P:z'],
];

echo "== namespaced tree ==\n";
foreach ($dom_snns_rows as [$label, $xml, $xpath]) {
    dom_snns_new($label, $xml, $xpath);
}

/* The 2004 tree declares nothing here and is not touched by this: its
 * saveXML() writes the prefix the node carries and leaves the caller to know
 * what it meant. */
echo "== 2004 tree ==\n";
foreach ($dom_snns_rows as [$label, $xml, $xpath]) {
    dom_snns_old($label, $xml, $xpath);
}

/* Saved whole, every binding is already in scope, so nothing is minted. */
echo "== whole document ==\n";
[$dom_snns_d] = dom_snns_at('<r xmlns:p="urn:p" xmlns:q="urn:q"><p:z q:k="1"><q:w/></p:z></r>', '//P:z');
echo $dom_snns_d->saveXml(), "\n";

/* The other face: an element in NO namespace under a default binding has to
 * have that binding cancelled, or the bytes read back into it. A CONSTRUCTED
 * one asks it of the serializer where a parsed one carries the answer itself. */
echo "== no namespace under a default ==\n";
$dom_snns_d3 = Dom\XMLDocument::createFromString('<r xmlns="urn:def"><a/></r>');
$dom_snns_d3->documentElement->appendChild($dom_snns_d3->createElement('z'));
$dom_snns_d3->documentElement->appendChild($dom_snns_d3->createElementNS(null, 'w'));
$dom_snns_d3->documentElement->firstElementChild->appendChild(
    $dom_snns_d3->createElementNS('urn:p', 'p:deep'));
echo $dom_snns_d3->saveXml(), "\n";
echo $dom_snns_d3->saveXml($dom_snns_d3->documentElement->lastElementChild), "\n";
echo $dom_snns_d3->saveXml($dom_snns_d3->documentElement), "\n";
/* Only a NON-empty default in scope is cancelled: prefixed ones bind nothing
 * an unprefixed name would pick up. */
$dom_snns_d4 = Dom\XMLDocument::createFromString('<r xmlns:p="urn:p"><a/></r>');
$dom_snns_d4->documentElement->appendChild($dom_snns_d4->createElement('z'));
echo $dom_snns_d4->saveXml(), "\n";

/* ...and the tree itself is unchanged by the dump: what the bytes gained is
 * undone, so a second read of the node answers what it always did. */
echo "== tree after the dump ==\n";
[$dom_snns_d2, $dom_snns_n2] = dom_snns_at('<r xmlns:p="urn:p"><p:z/></r>', '//P:z');
$dom_snns_d2->saveXml($dom_snns_n2);
var_dump($dom_snns_n2->prefix, $dom_snns_n2->namespaceURI,
    $dom_snns_n2->getAttributeNames(), count($dom_snns_n2->attributes));
echo $dom_snns_d2->saveXml($dom_snns_n2), "\n";
?>
--EXPECT--
== namespaced tree ==
prefixed, anc binding          <p:z xmlns:p="urn:p"/>
default ns element             <z xmlns="urn:p"/>
no-ns elem under default       <z xmlns=""/>
elem+attr same uri             <p:z xmlns:p="urn:p" p:k="1"/>
inner shadows prefix           <p:z xmlns:p="urn:p"><p:w xmlns:p="urn:q"/></p:z>
default + prefixed both        <z xmlns="urn:p" xmlns:q="urn:q" q:k="1"/>
elem ns unused deeper          <p:z xmlns:p="urn:p"><q:w xmlns:q="urn:q"/></p:z>
anc default, elem prefixed     <p:z xmlns:p="urn:p"><w xmlns="urn:q"/></p:z>
== 2004 tree ==
prefixed, anc binding          <p:z/>
default ns element             <z/>
no-ns elem under default       <z xmlns=""/>
elem+attr same uri             <p:z p:k="1"/>
inner shadows prefix           <p:z><p:w xmlns:p="urn:q"/></p:z>
default + prefixed both        <z q:k="1"/>
elem ns unused deeper          <p:z><q:w/></p:z>
anc default, elem prefixed     <p:z><w/></p:z>
== whole document ==
<?xml version="1.0" encoding="UTF-8"?>
<r xmlns:p="urn:p" xmlns:q="urn:q"><p:z q:k="1"><q:w/></p:z></r>
== no namespace under a default ==
<?xml version="1.0" encoding="UTF-8"?>
<r xmlns="urn:def"><a><p:deep xmlns:p="urn:p"/></a><z xmlns=""/><w xmlns=""/></r>
<w/>
<r xmlns="urn:def"><a><p:deep xmlns:p="urn:p"/></a><z xmlns=""/><w xmlns=""/></r>
<?xml version="1.0" encoding="UTF-8"?>
<r xmlns:p="urn:p"><a/><z/></r>
== tree after the dump ==
string(1) "p"
string(5) "urn:p"
array(0) {
}
int(0)
<p:z xmlns:p="urn:p"/>
