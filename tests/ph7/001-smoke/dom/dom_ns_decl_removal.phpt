--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Each tree addresses a namespace declaration through removeAttributeNS() by its own half of the pair
--FILE--
<?php
$dom_ndr_X = 'http://www.w3.org/2000/xmlns/';

function dom_ndr_mk(bool $modern, string $xml)
{
    if ($modern) {
        return Dom\XMLDocument::createFromString($xml);
    }
    $d = new DOMDocument;
    $d->loadXML($xml);
    return $d;
}

function dom_ndr_try(string $label, bool $modern, string $xml, ?string $uri, string $local)
{
    $d = dom_ndr_mk($modern, $xml);
    $r = $d->documentElement;
    $r->removeAttributeNS($uri, $local);
    printf("%-38s %s\n", $label, $r->C14N());
}

// php 8.4's element carries its declarations as attributes in the xmlns
// namespace, so THAT is the namespace half of the pair there. The 2004 element
// carries none, and answers to the URI the prefix actually binds instead --
// which is why the same call removes a declaration on one tree and nothing at
// all on the other.
$dom_ndr_doc = '<r xmlns:a="urn:a" id="k"><c/></r>';
dom_ndr_try('legacy (urn:a, a)',  false, $dom_ndr_doc, 'urn:a', 'a');
dom_ndr_try('legacy (xmlns ns, a)', false, $dom_ndr_doc, $dom_ndr_X, 'a');
dom_ndr_try('modern (urn:a, a)',  true,  $dom_ndr_doc, 'urn:a', 'a');
dom_ndr_try('modern (xmlns ns, a)', true, $dom_ndr_doc, $dom_ndr_X, 'a');

// The DEFAULT declaration is named by the local name `xmlns` on the modern tree
// and by the EMPTY local name on the 2004 one; each tree answers to nothing
// else.
$dom_ndr_dflt = '<r xmlns="urn:d" a="1"><c/></r>';
dom_ndr_try('legacy default ("")',  false, $dom_ndr_dflt, 'urn:d', '');
dom_ndr_try('legacy default (xmlns)', false, $dom_ndr_dflt, 'urn:d', 'xmlns');
dom_ndr_try('modern default (xmlns)', true, $dom_ndr_dflt, $dom_ndr_X, 'xmlns');
dom_ndr_try('modern default ("")',  true,  $dom_ndr_dflt, $dom_ndr_X, '');

// Removing it is not the same act on the two trees. The modern one drops the
// declaration and leaves the subtree in the namespace, so a serializer puts the
// declaration back where it is used; the 2004 one ELIMINATES the binding, and
// every node below that resolved through it is left in no namespace at all.
$dom_ndr_used = '<r xmlns:a="urn:a"><b><a:c a:x="1"><a:d/></a:c></b></r>';
dom_ndr_try('legacy used', false, $dom_ndr_used, 'urn:a', 'a');
dom_ndr_try('modern used', true,  $dom_ndr_used, $dom_ndr_X, 'a');

$dom_ndr_d = dom_ndr_mk(false, '<r xmlns:a="urn:a"><a:c/></r>');
$dom_ndr_c = $dom_ndr_d->documentElement->firstElementChild;
$dom_ndr_d->documentElement->removeAttributeNS('urn:a', 'a');
var_dump($dom_ndr_c->namespaceURI, $dom_ndr_c->prefix, $dom_ndr_c->nodeName);
$dom_ndr_d = dom_ndr_mk(true, '<r xmlns:a="urn:a"><a:c/></r>');
$dom_ndr_c = $dom_ndr_d->documentElement->firstElementChild;
$dom_ndr_d->documentElement->removeAttributeNS($dom_ndr_X, 'a');
var_dump($dom_ndr_c->namespaceURI, $dom_ndr_c->prefix, $dom_ndr_c->nodeName);

// Elimination is by binding, not by URI: a second prefix bound to the same URI
// keeps its own declaration.
dom_ndr_try('legacy twin binding', false, '<r xmlns:a="urn:a" xmlns:b="urn:a"><a:c/><b:e/></r>', 'urn:a', 'a');

// A declaration this element makes SCREENS the whole call on the 2004 tree: a
// URI that is not the one it binds stops it dead, and the attribute the same
// pair names is left in place too.
dom_ndr_try('legacy screened by the decl', false, '<r xmlns:a="urn:a" xmlns:z="urn:z" z:a="v"/>', 'urn:z', 'a');
dom_ndr_try('legacy screened, null uri', false, '<r xmlns:a="urn:a" a="plain"/>', null, 'a');
// ...and where the pair names both, the declaration goes AND the attribute does.
dom_ndr_try('legacy decl and attribute', false, '<r xmlns:a="urn:a" a:a="v" b="w"/>', 'urn:a', 'a');
// A declaration made by an ANCESTOR is not this element's to remove, so the
// attribute path runs as usual.
$dom_ndr_d = dom_ndr_mk(false, '<r xmlns:a="urn:a"><c a:v="1"/></r>');
$dom_ndr_d->documentElement->firstElementChild->removeAttributeNS('urn:a', 'v');
echo $dom_ndr_d->documentElement->C14N(), "\n";

// The same pair on the read doors: the modern tree answers about its default
// declaration under the local name `xmlns`, the 2004 tree reads that name as a
// prefix like any other.
foreach ([false, true] as $dom_ndr_m) {
    $dom_ndr_r = dom_ndr_mk($dom_ndr_m, '<r xmlns="urn:d" xmlns:p="urn:p"/>')->documentElement;
    foreach (['xmlns', 'p', 'q'] as $dom_ndr_ln) {
        printf("%s has(%-5s)=%d get=%s node=%s\n", $dom_ndr_m ? 'modern' : 'legacy', $dom_ndr_ln,
            (int)$dom_ndr_r->hasAttributeNS($dom_ndr_X, $dom_ndr_ln),
            var_export($dom_ndr_r->getAttributeNS($dom_ndr_X, $dom_ndr_ln), true),
            var_export($dom_ndr_r->getAttributeNodeNS($dom_ndr_X, $dom_ndr_ln)?->nodeName, true));
    }
}
?>
--EXPECT--
legacy (urn:a, a)                      <r id="k"><c></c></r>
legacy (xmlns ns, a)                   <r xmlns:a="urn:a" id="k"><c></c></r>
modern (urn:a, a)                      <r xmlns:a="urn:a" id="k"><c></c></r>
modern (xmlns ns, a)                   <r id="k"><c></c></r>
legacy default ("")                    <r a="1"><c></c></r>
legacy default (xmlns)                 <r xmlns="urn:d" a="1"><c></c></r>
modern default (xmlns)                 <r a="1"><c></c></r>
modern default ("")                    <r xmlns="urn:d" a="1"><c></c></r>
legacy used                            <r><b><c x="1"><d></d></c></b></r>
modern used                            <r><b><a:c xmlns:a="urn:a" a:x="1"><a:d></a:d></a:c></b></r>
NULL
string(0) ""
string(1) "c"
string(5) "urn:a"
string(1) "a"
string(3) "a:c"
legacy twin binding                    <r xmlns:b="urn:a"><c></c><b:e></b:e></r>
legacy screened by the decl            <r xmlns:a="urn:a" xmlns:z="urn:z" z:a="v"></r>
legacy screened, null uri              <r xmlns:a="urn:a" a="plain"></r>
legacy decl and attribute              <r b="w"></r>
<r xmlns:a="urn:a"><c></c></r>
legacy has(xmlns)=0 get='' node=NULL
legacy has(p    )=1 get='urn:p' node='xmlns:p'
legacy has(q    )=0 get='' node=NULL
modern has(xmlns)=1 get='urn:d' node='xmlns'
modern has(p    )=1 get='urn:p' node='xmlns:p'
modern has(q    )=0 get=NULL node=NULL
