--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced saver declares the binding an attribute only carries
--FILE--
<?php
// php 8.4's tree parks an attribute's binding rather than declaring it, so an
// attribute made by createAttributeNS() carries a prefix nothing on its element
// declares. The saver mints the declaration as it writes, and the prefix it
// writes is the SAVER's choice, not the attribute's: an in-scope prefixed
// binding of the URI is written under that prefix, otherwise the attribute's
// own prefix is declared here, and otherwise `ns1`. A default declaration is
// never reused and never satisfies the test -- an unprefixed attribute name is
// in no namespace whatever is in scope. An ancestor's binding of the prefix is
// shadowed rather than stepped over; only a declaration on this very element
// forces `ns1`. The tree itself never changes: `prefix` reads back what the
// attribute was made with whatever the bytes say.
$dom_msdan_put = static function ($xml, $uri, $qname) {
    $doc = Dom\XMLDocument::createFromString($xml);
    $attr = $doc->createAttributeNS($uri, $qname);
    $doc->documentElement->setAttributeNodeNS($attr);
    $attr->value = 'V';
    return [$doc, $attr];
};
$dom_msdan_cases = [
    'nothing bound'            => ['<r/>',                '<urn:x>', 'p:foo'],
    'same URI, other prefix'   => ['<r xmlns:q="urn:x"/>', '<urn:x>', 'p:foo'],
    'prefix bound, other URI'  => ['<r xmlns:p="urn:1"/>', '<urn:x>', 'p:foo'],
    'default binds the URI'    => ['<r xmlns="urn:x"/>',   '<urn:x>', 'p:foo'],
    'no prefix asked for'      => ['<r/>',                '<urn:x>', 'foo'],
    'default binds, no prefix' => ['<r xmlns="urn:x"/>',   '<urn:x>', 'foo'],
    'the xml prefix'           => ['<r/>', '<http://www.w3.org/XML/1998/namespace>', 'xml:lang'],
];
foreach ($dom_msdan_cases as $dom_msdan_label => $dom_msdan_case) {
    [$dom_msdan_xml, $dom_msdan_uri, $dom_msdan_q] = $dom_msdan_case;
    [$dom_msdan_doc, $dom_msdan_attr] =
        $dom_msdan_put($dom_msdan_xml, trim($dom_msdan_uri, '<>'), $dom_msdan_q);
    printf(
        "%-26s %-34s prefix=%s\n",
        $dom_msdan_label,
        trim(explode("\n", $dom_msdan_doc->saveXml())[1]),
        var_export($dom_msdan_attr->prefix, true)
    );
}
// A prefix already minted for one attribute is reused by the next, and two
// elements each declare their own when the same prefix names two URIs.
$dom_msdan_two = Dom\XMLDocument::createFromString('<r/>');
$dom_msdan_a = $dom_msdan_two->createAttributeNS('urn:x', 'p:foo');
$dom_msdan_two->documentElement->setAttributeNodeNS($dom_msdan_a);
$dom_msdan_a->value = '1';
$dom_msdan_b = $dom_msdan_two->createAttributeNS('urn:x', 'z:bar');
$dom_msdan_two->documentElement->setAttributeNodeNS($dom_msdan_b);
$dom_msdan_b->value = '2';
echo 'reused:  ', trim(explode("\n", $dom_msdan_two->saveXml())[1]), "\n";

$dom_msdan_sh = Dom\XMLDocument::createFromString('<r><c/></r>');
$dom_msdan_c = $dom_msdan_sh->createAttributeNS('urn:1', 'p:foo');
$dom_msdan_sh->documentElement->setAttributeNodeNS($dom_msdan_c);
$dom_msdan_c->value = '1';
$dom_msdan_d = $dom_msdan_sh->createAttributeNS('urn:2', 'p:bar');
$dom_msdan_sh->documentElement->firstChild->setAttributeNodeNS($dom_msdan_d);
$dom_msdan_d->value = '2';
echo 'shadow:  ', trim(explode("\n", $dom_msdan_sh->saveXml())[1]), "\n";

// A node dump counts "in scope" from the NODE, so it mints its own declaration
// where the whole document writes the ancestor's prefix.
$dom_msdan_nd = Dom\XMLDocument::createFromString('<r xmlns:q="urn:x"><c/></r>');
$dom_msdan_e = $dom_msdan_nd->createAttributeNS('urn:x', 'p:foo');
$dom_msdan_nd->documentElement->firstChild->setAttributeNodeNS($dom_msdan_e);
$dom_msdan_e->value = 'V';
echo 'node:    ', $dom_msdan_nd->saveXml($dom_msdan_nd->documentElement->firstChild), "\n";
echo 'whole:   ', trim(explode("\n", $dom_msdan_nd->saveXml())[1]), "\n";
// Saving does not change the tree, so the second save answers the first.
echo 'again:   ', trim(explode("\n", $dom_msdan_nd->saveXml())[1]), "\n";
var_dump($dom_msdan_e->prefix, $dom_msdan_e->namespaceURI);
// ...and the bytes parse back, which is the whole point of declaring them.
$dom_msdan_back = Dom\XMLDocument::createFromString($dom_msdan_nd->saveXml());
echo 'reparse: ', $dom_msdan_back->documentElement->firstChild
    ->getAttributeNS('urn:x', 'foo'), "\n";
?>
--EXPECT--
nothing bound              <r xmlns:p="urn:x" p:foo="V"/>     prefix='p'
same URI, other prefix     <r xmlns:q="urn:x" q:foo="V"/>     prefix='p'
prefix bound, other URI    <r xmlns:p="urn:1" xmlns:ns1="urn:x" ns1:foo="V"/> prefix='p'
default binds the URI      <r xmlns="urn:x" xmlns:p="urn:x" p:foo="V"/> prefix='p'
no prefix asked for        <r xmlns:ns1="urn:x" ns1:foo="V"/> prefix=NULL
default binds, no prefix   <r xmlns="urn:x" xmlns:ns1="urn:x" ns1:foo="V"/> prefix=NULL
the xml prefix             <r xml:lang="V"/>                  prefix='xml'
reused:  <r xmlns:p="urn:x" p:foo="1" p:bar="2"/>
shadow:  <r xmlns:p="urn:1" p:foo="1"><c xmlns:p="urn:2" p:bar="2"/></r>
node:    <c xmlns:p="urn:x" p:foo="V"/>
whole:   <r xmlns:q="urn:x"><c q:foo="V"/></r>
again:   <r xmlns:q="urn:x"><c q:foo="V"/></r>
string(1) "p"
string(5) "urn:x"
reparse: V
