--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A namespaced attribute write never leaves the attribute in the default namespace
--FILE--
<?php
/* `setAttributeNS` resolves its namespace by the 2004 tree's rules on the 2004
 * tree and by php 8.4's own on the namespaced one, and the two disagree about
 * an attribute that asks for no prefix. The old tree may declare the DEFAULT
 * namespace -- which says nothing about an attribute, since an unprefixed
 * attribute is in no namespace at all -- where the namespaced one always
 * invents a prefix, spelt `ns1`, `ns2`, ... rather than `default`, `default1`,
 * and numbered against the declarations THIS element makes so that an `ns1` an
 * ancestor binds is shadowed rather than stepped over.
 *
 * The name is judged by the namespaced tree's created-attribute grammar too:
 * a QName whatever namespace came with it, every grammar failure the Invalid
 * Character Error, and the two reserved rules checked on the NAME -- so the
 * `xml` prefix under another URI is refused here even where the 2004 tree
 * quietly writes it under whatever prefix that URI already had. */
const XMLNS = 'http://www.w3.org/2000/xmlns/';
const XML   = 'http://www.w3.org/XML/1998/namespace';

function row(string $label, string $xml, ?string $uri, string $qname): void
{
	foreach (['modern', '2004'] as $tree) {
		if ($tree === 'modern') {
			$doc = Dom\XMLDocument::createFromString($xml);
		} else {
			$doc = new DOMDocument();
			$doc->loadXML($xml);
		}
		try {
			$doc->documentElement->setAttributeNS($uri, $qname, 'v');
			$out = trim($doc->saveXml($doc->documentElement));
		} catch (Throwable $e) {
			$out = get_class($e) . '/' . $e->getCode() . ': ' . $e->getMessage();
		}
		printf("%-24s %-6s %s\n", $label, $tree, $out);
	}
}

/* The invented prefix, and where its number comes from. */
row('free',            '<r/>',                                    'urn:u', 'z');
row('ns1 on self',     '<r xmlns:ns1="urn:o"/>',                   'urn:u', 'z');
row('ns1+ns2 on self', '<r xmlns:ns1="urn:o" xmlns:ns2="urn:o2"/>', 'urn:u', 'z');
row('prefix taken',    '<r xmlns:q="urn:o"/>',                     'urn:u', 'q:z');

/* A binding already in scope is reused only when it can spell an attribute. */
row('default binds it', '<r xmlns="urn:u"/>',                      'urn:u', 'z');
row('prefix binds it',  '<r xmlns:p="urn:u"/>',                    'urn:u', 'z');
row('prefix asked free', '<r/>',                                   'urn:u', 'q:z');

/* The reserved prefixes, and the one URI each may spell. */
row('xml pfx, free uri',  '<r/>',                'urn:u', 'xml:id');
row('xml pfx, bound uri', '<r xmlns:p="urn:u"/>', 'urn:u', 'xml:id');
row('xml pfx, xml uri',   '<r/>',                XML,     'xml:lang');
row('xmlns pfx, free uri', '<r/>',               'urn:u', 'xmlns:z');
row('xmlns pfx, xmlns uri', '<r/>',              XMLNS,   'xmlns:z');

/* The grammar, and which of the two codes each failure takes. */
row('empty name',      '<r/>', 'urn:u', '');
row('two colons',      '<r/>', 'urn:u', 'a:b:c');
row('space in name',   '<r/>', null,    'x y');
row('leading colon',   '<r/>', null,    ':x');
row('bare xmlns',      '<r/>', null,    'xmlns');
row('prefix, no uri',  '<r/>', null,    'q:z');

/* An ancestor's `ns1` is shadowed, not stepped over. */
$doc = Dom\XMLDocument::createFromString('<r xmlns:ns1="urn:o"><a/></r>');
$doc->documentElement->firstElementChild->setAttributeNS('urn:u', 'z', 'v');
echo trim($doc->saveXml($doc->documentElement)), "\n";

/* A second attribute of the same URI reuses the prefix the first invented. */
$doc = Dom\XMLDocument::createFromString('<r/>');
$doc->documentElement->setAttributeNS('urn:u', 'z', '1');
$doc->documentElement->setAttributeNS('urn:u', 'y', '2');
echo trim($doc->saveXml($doc->documentElement)), "\n";
?>
--EXPECT--
free                     modern <r xmlns:ns1="urn:u" ns1:z="v"/>
free                     2004   <r xmlns="urn:u" z="v"/>
ns1 on self              modern <r xmlns:ns1="urn:o" xmlns:ns2="urn:u" ns2:z="v"/>
ns1 on self              2004   <r xmlns:ns1="urn:o" xmlns="urn:u" z="v"/>
ns1+ns2 on self          modern <r xmlns:ns1="urn:o" xmlns:ns2="urn:o2" xmlns:ns3="urn:u" ns3:z="v"/>
ns1+ns2 on self          2004   <r xmlns:ns1="urn:o" xmlns:ns2="urn:o2" xmlns="urn:u" z="v"/>
prefix taken             modern <r xmlns:q="urn:o" xmlns:ns1="urn:u" ns1:z="v"/>
prefix taken             2004   <r xmlns:q="urn:o" xmlns:default="urn:u" default:z="v"/>
default binds it         modern <r xmlns="urn:u" xmlns:ns1="urn:u" ns1:z="v"/>
default binds it         2004   <r xmlns="urn:u" xmlns:default="urn:u" default:z="v"/>
prefix binds it          modern <r xmlns:p="urn:u" p:z="v"/>
prefix binds it          2004   <r xmlns:p="urn:u" p:z="v"/>
prefix asked free        modern <r xmlns:q="urn:u" q:z="v"/>
prefix asked free        2004   <r xmlns:q="urn:u" q:z="v"/>
xml pfx, free uri        modern DOMException/14: Namespace Error
xml pfx, free uri        2004   DOMException/14: Namespace Error
xml pfx, bound uri       modern DOMException/14: Namespace Error
xml pfx, bound uri       2004   <r xmlns:p="urn:u" p:id="v"/>
xml pfx, xml uri         modern <r xml:lang="v"/>
xml pfx, xml uri         2004   <r xml:lang="v"/>
xmlns pfx, free uri      modern DOMException/14: Namespace Error
xmlns pfx, free uri      2004   DOMException/14: Namespace Error
xmlns pfx, xmlns uri     modern <r xmlns:z="v"/>
xmlns pfx, xmlns uri     2004   <r xmlns:z="v"/>
empty name               modern DOMException/5: Invalid Character Error
empty name               2004   ValueError/0: DOMElement::setAttributeNS(): Argument #2 ($qualifiedName) must not be empty
two colons               modern DOMException/5: Invalid Character Error
two colons               2004   DOMException/14: Namespace Error
space in name            modern DOMException/5: Invalid Character Error
space in name            2004   DOMException/5: Invalid Character Error
leading colon            modern DOMException/5: Invalid Character Error
leading colon            2004   <r :x="v"/>
bare xmlns               modern DOMException/14: Namespace Error
bare xmlns               2004   <r xmlns="v"/>
prefix, no uri           modern DOMException/14: Namespace Error
prefix, no uri           2004   DOMException/14: Namespace Error
<r xmlns:ns1="urn:o"><a xmlns:ns1="urn:u" ns1:z="v"/></r>
<r xmlns:ns1="urn:u" ns1:z="1" ns1:y="2"/>
