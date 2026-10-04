--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Removing a namespace declaration re-declares it where it is still used on the 2004 tree only
--FILE--
<?php
function dom_ndrs_mk(bool $modern, string $xml)
{
    if ($modern) {
        return Dom\XMLDocument::createFromString($xml);
    }
    $d = new DOMDocument;
    $d->loadXML($xml);
    return $d;
}

function dom_ndrs_try(string $label, bool $modern, string $xml, string $path, string $name)
{
    $d = dom_ndrs_mk($modern, $xml);
    $n = $d->documentElement;
    for ($i = 0; $i < strlen($path); $i++) {
        $n = $n->firstElementChild;
    }
    $v = $n->removeAttribute($name);
    printf("%-32s %-6s %-6s %s\n", $label, $modern ? 'modern' : 'legacy',
        var_export($v, true), $d->documentElement->C14N());
}

// The 2004 tree gives a removed binding back to whatever still resolves through
// it: the declaration lands on the node that NEEDS it, which is the element
// itself when the whole subtree is in that namespace and a descendant when the
// element is not.  php 8.4's tree respells nothing at all -- the nodes still
// ANSWER the namespace, and the serializer writes no declaration for a binding
// declared nowhere.
foreach ([false, true] as $dom_ndrs_m) {
    dom_ndrs_try('the whole tree uses it', $dom_ndrs_m, '<r xmlns="urn:d"><c><g/></c></r>', '', 'xmlns');
    dom_ndrs_try('only the root uses it', $dom_ndrs_m, '<r xmlns:p="urn:p" xmlns="urn:d"><p:c/></r>', '', 'xmlns');
    dom_ndrs_try('the root is prefixed', $dom_ndrs_m, '<p:r xmlns:p="urn:p" xmlns="urn:d"><c/></p:r>', '', 'xmlns');
    dom_ndrs_try('a prefixed one, still used', $dom_ndrs_m, '<r xmlns:p="urn:p"><p:c p:x="1"/></r>', '', 'xmlns:p');
    dom_ndrs_try('a prefixed one, used by none', $dom_ndrs_m, '<r xmlns:p="urn:p"><c/></r>', '', 'xmlns:p');
}

// A binding of the same URI under ANOTHER prefix is not a spelling this node
// may take: reusing it by URI alone re-prefixed the element and its whole
// subtree, where php leaves both spelled as they were written.
dom_ndrs_try('a twin binds the URI here', false, '<r xmlns="urn:d" xmlns:q="urn:d"><c/></r>', '', 'xmlns');
dom_ndrs_try('a twin binds it above', false, '<r xmlns:q="urn:d"><m xmlns="urn:d"><c/></m></r>', 'm', 'xmlns');
dom_ndrs_try('a twin above, prefixed', false, '<r xmlns:q="urn:d"><m xmlns:p="urn:d"><p:c/></m></r>', 'm', 'xmlns:p');

// The SAME prefix bound to the SAME URI above is, though -- so an ancestor's
// declaration absorbs the node and nothing new is written.
dom_ndrs_try('an ancestor binds it alike', false, '<r xmlns:p="urn:d"><m xmlns:p="urn:d"><p:c/></m></r>', 'm', 'xmlns:p');
dom_ndrs_try('an ancestor binds the default', false, '<r xmlns="urn:d"><m xmlns="urn:d"><c/></m></r>', 'm', 'xmlns');
// ...and one that binds the prefix to something ELSE is shadowed on the spot.
dom_ndrs_try('the prefix binds elsewhere', false, '<r xmlns:p="urn:z"><m xmlns:p="urn:d"><p:c/></m></r>', 'm', 'xmlns:p');
// An attribute is a user like any other, and its declaration lands on the
// element that carries it.
dom_ndrs_try('an attribute is the only user', false, '<r xmlns:p="urn:p"><m xmlns:q="urn:q" q:k="1"><c/></m></r>', 'm', 'xmlns:q');
// The users deeper than the site inherit the one declaration.
dom_ndrs_try('the users are deeper', false, '<r xmlns:p="urn:p"><m xmlns="urn:d"><c><g/></c></m></r>', 'm', 'xmlns');
// Only a deep node uses it, so only that node declares it.
dom_ndrs_try('only a deep node uses it', false, '<p:r xmlns:p="urn:p" xmlns="urn:d"><p:m><c/></p:m></p:r>', '', 'xmlns');

// The node left behind answers the namespace on both trees, declaration or no
// declaration.
foreach ([false, true] as $dom_ndrs_m) {
    $dom_ndrs_d = dom_ndrs_mk($dom_ndrs_m, '<r xmlns="urn:d"><c/></r>');
    $dom_ndrs_c = $dom_ndrs_d->documentElement->firstElementChild;
    $dom_ndrs_d->documentElement->removeAttribute('xmlns');
    printf("%s child: %s %s %s\n", $dom_ndrs_m ? 'modern' : 'legacy',
        var_export($dom_ndrs_c->namespaceURI, true),
        var_export($dom_ndrs_c->prefix, true), $dom_ndrs_c->nodeName);
}

// toggleAttribute removes a declaration through the same door.
$dom_ndrs_d = dom_ndrs_mk(false, '<r xmlns="urn:d"><c/></r>');
var_dump($dom_ndrs_d->documentElement->toggleAttribute('xmlns'));
echo $dom_ndrs_d->documentElement->C14N(), "\n";
?>
--EXPECT--
the whole tree uses it           legacy true   <r xmlns="urn:d"><c><g></g></c></r>
only the root uses it            legacy true   <r xmlns="urn:d" xmlns:p="urn:p"><p:c></p:c></r>
the root is prefixed             legacy true   <p:r xmlns:p="urn:p"><c xmlns="urn:d"></c></p:r>
a prefixed one, still used       legacy true   <r><p:c xmlns:p="urn:p" p:x="1"></p:c></r>
a prefixed one, used by none     legacy true   <r><c></c></r>
the whole tree uses it           modern NULL   <r><c><g></g></c></r>
only the root uses it            modern NULL   <r xmlns:p="urn:p"><p:c></p:c></r>
the root is prefixed             modern NULL   <p:r xmlns:p="urn:p"><c></c></p:r>
a prefixed one, still used       modern NULL   <r><p:c xmlns:p="urn:p" p:x="1"></p:c></r>
a prefixed one, used by none     modern NULL   <r><c></c></r>
a twin binds the URI here        legacy true   <r xmlns="urn:d" xmlns:q="urn:d"><c></c></r>
a twin binds it above            legacy true   <r xmlns:q="urn:d"><m xmlns="urn:d"><c></c></m></r>
a twin above, prefixed           legacy true   <r xmlns:q="urn:d"><m><p:c xmlns:p="urn:d"></p:c></m></r>
an ancestor binds it alike       legacy true   <r xmlns:p="urn:d"><m><p:c></p:c></m></r>
an ancestor binds the default    legacy true   <r xmlns="urn:d"><m><c></c></m></r>
the prefix binds elsewhere       legacy true   <r xmlns:p="urn:z"><m><p:c xmlns:p="urn:d"></p:c></m></r>
an attribute is the only user    legacy true   <r xmlns:p="urn:p"><m xmlns:q="urn:q" q:k="1"><c></c></m></r>
the users are deeper             legacy true   <r xmlns:p="urn:p"><m xmlns="urn:d"><c><g></g></c></m></r>
only a deep node uses it         legacy true   <p:r xmlns:p="urn:p"><p:m><c xmlns="urn:d"></c></p:m></p:r>
legacy child: 'urn:d' '' c
modern child: 'urn:d' NULL c
bool(false)
<r xmlns="urn:d"><c></c></r>
