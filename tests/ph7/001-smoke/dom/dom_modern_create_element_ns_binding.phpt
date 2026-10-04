--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced element factory binds the prefix asked for and declares no default
--FILE--
<?php
// php 8.4's Dom\Document::createElementNS() is a smaller rule than the 2004
// door's: it binds exactly the prefix it was given and reuses nothing, so a
// document that already binds the URI under `p` still answers an unprefixed
// name, and the XML namespace does not come back spelled `xml:`. And an
// unprefixed binding is declared NOWHERE -- the element carries the namespace
// and no `xmlns=` exists for it, at the node, at every ancestor and after an
// insert -- while whatever default declaration IS in scope is the one that
// names the element when it is canonicalized.
$dom_mcens_make = static function (string $tree, string $xml) {
    if ($tree === 'modern') {
        return Dom\XMLDocument::createFromString($xml);
    }
    $d = new DOMDocument();
    $d->loadXML($xml);
    return $d;
};
$dom_mcens_row = static function (string $label, string $tree, $d, $e): void {
    printf(
        "%-22s %-6s name=%-8s prefix=%-8s uri=%-8s %s\n",
        $label,
        $tree,
        $e->nodeName,
        var_export($e->prefix, true),
        var_export($e->namespaceURI, true),
        $d->C14N()
    );
};

// What the factory binds, against every kind of declaration already in scope.
foreach ([
    'no declaration'   => ['<r/>', 'urn:e', 'x'],
    'default in scope' => ['<r xmlns="urn:d"/>', 'urn:d', 'x'],
    'prefix in scope'  => ['<r xmlns:p="urn:d"/>', 'urn:d', 'x'],
    'asks its own'     => ['<r xmlns:p="urn:d"/>', 'urn:d', 'q:x'],
    'other URI'        => ['<r xmlns="urn:d"/>', 'urn:e', 'x'],
    'the XML namespace' => ['<r/>', 'http://www.w3.org/XML/1998/namespace', 'x'],
] as $dom_mcens_label => [$dom_mcens_xml, $dom_mcens_uri, $dom_mcens_qn]) {
    foreach (['modern', 'legacy'] as $dom_mcens_tree) {
        $dom_mcens_d = $dom_mcens_make($dom_mcens_tree, $dom_mcens_xml);
        $dom_mcens_e = $dom_mcens_d->createElementNS($dom_mcens_uri, $dom_mcens_qn);
        $dom_mcens_d->documentElement->appendChild($dom_mcens_e);
        $dom_mcens_row($dom_mcens_label, $dom_mcens_tree, $dom_mcens_d, $dom_mcens_e);
    }
}

// The undeclared default, through every face that can see it.
$dom_mcens_d1 = Dom\XMLDocument::createEmpty();
$dom_mcens_s = $dom_mcens_d1->createElementNS('urn:d', 's');
$dom_mcens_d1->appendChild($dom_mcens_s);
printf("root alone            %s\n", $dom_mcens_s->C14N());
printf("document              %s\n", $dom_mcens_d1->C14N());
printf("exclusive             %s\n", $dom_mcens_d1->C14N(true));
$dom_mcens_c = $dom_mcens_d1->createElementNS('urn:d', 'c');
$dom_mcens_s->appendChild($dom_mcens_c);
printf("same URI child        %s\n", $dom_mcens_d1->C14N());
printf("that child alone      %s\n", $dom_mcens_c->C14N());
$dom_mcens_s->appendChild($dom_mcens_d1->createElementNS('urn:e', 'e'));
$dom_mcens_s->appendChild($dom_mcens_d1->createElement('plain'));
$dom_mcens_s->appendChild($dom_mcens_d1->createElementNS('urn:p', 'q:r'));
printf("mixed children        %s\n", $dom_mcens_d1->C14N());
$dom_mcens_s->setAttribute('k', '1');
$dom_mcens_s->setAttributeNS('urn:p', 'p:z', '2');
printf("after attribute write %s\n", $dom_mcens_d1->C14N());

// The namespace still answers every question about itself.
var_dump(
    $dom_mcens_s->namespaceURI,
    $dom_mcens_s->prefix,
    $dom_mcens_s->nodeName,
    $dom_mcens_c->isDefaultNamespace('urn:d'),
    $dom_mcens_c->lookupNamespaceURI(null),
    $dom_mcens_s->attributes->length
);

// A declaration that IS in scope is what names the element instead.
$dom_mcens_d2 = Dom\XMLDocument::createFromString('<r xmlns="urn:d"><a/></r>');
$dom_mcens_d2->documentElement->appendChild($dom_mcens_d2->createElementNS('urn:d', 'n'));
$dom_mcens_d2->documentElement->appendChild($dom_mcens_d2->createElementNS('urn:e', 'm'));
printf("under a declaration   %s\n", $dom_mcens_d2->C14N());

// The 2004 door declares both, and reuses a binding of the same URI in scope.
$dom_mcens_d3 = new DOMDocument();
$dom_mcens_d3->loadXML('<r/>');
$dom_mcens_d3->documentElement->appendChild($dom_mcens_d3->createElementNS('urn:d', 's'));
printf("2004 door             %s\n", $dom_mcens_d3->C14N());
printf("2004 saveXML          %s\n", $dom_mcens_d3->saveXML($dom_mcens_d3->documentElement));
--EXPECT--
no declaration         modern name=x        prefix=NULL     uri='urn:e'  <r><x></x></r>
no declaration         legacy name=x        prefix=''       uri='urn:e'  <r><x xmlns="urn:e"></x></r>
default in scope       modern name=x        prefix=NULL     uri='urn:d'  <r xmlns="urn:d"><x></x></r>
default in scope       legacy name=x        prefix=''       uri='urn:d'  <r xmlns="urn:d"><x></x></r>
prefix in scope        modern name=x        prefix=NULL     uri='urn:d'  <r xmlns:p="urn:d"><x></x></r>
prefix in scope        legacy name=p:x      prefix='p'      uri='urn:d'  <r xmlns:p="urn:d"><p:x></p:x></r>
asks its own           modern name=q:x      prefix='q'      uri='urn:d'  <r xmlns:p="urn:d"><q:x xmlns:q="urn:d"></q:x></r>
asks its own           legacy name=q:x      prefix='q'      uri='urn:d'  <r xmlns:p="urn:d"><q:x xmlns:q="urn:d"></q:x></r>
other URI              modern name=x        prefix=NULL     uri='urn:e'  <r xmlns="urn:d"><x></x></r>
other URI              legacy name=x        prefix=''       uri='urn:e'  <r xmlns="urn:d"><x xmlns="urn:e"></x></r>
the XML namespace      modern name=x        prefix=NULL     uri='http://www.w3.org/XML/1998/namespace' <r><x></x></r>
the XML namespace      legacy name=xml:x    prefix='xml'    uri='http://www.w3.org/XML/1998/namespace' <r><xml:x></xml:x></r>
root alone            <s></s>
document              <s></s>
exclusive             <s></s>
same URI child        <s><c></c></s>
that child alone      <c></c>
mixed children        <s><c></c><e></e><plain></plain><q:r xmlns:q="urn:p"></q:r></s>
after attribute write <s xmlns:p="urn:p" k="1" p:z="2"><c></c><e></e><plain></plain><q:r xmlns:q="urn:p"></q:r></s>
string(5) "urn:d"
NULL
string(1) "s"
bool(true)
string(5) "urn:d"
int(2)
under a declaration   <r xmlns="urn:d"><a></a><n></n><m></m></r>
2004 door             <r><s xmlns="urn:d"></s></r>
2004 saveXML          <r><s xmlns="urn:d"/></r>
