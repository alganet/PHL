--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced document imports, adopts and crosses a 2004 node into its own tree
--FILE--
<?php
$dom_mimp_say = static function (string $label, callable $f): void {
    try {
        $r = $f();
        echo $label, ' => ', is_object($r) ? get_class($r) : var_export($r, true), "\n";
    } catch (Throwable $e) {
        echo $label, ' => ', get_class($e), '(', $e->getCode(), '): ', $e->getMessage(), "\n";
    }
};
$dom_mimp_src = Dom\XMLDocument::createFromString('<r xmlns:p="urn:p"><c p:a="1"><g/></c></r>');
$dom_mimp_node = $dom_mimp_src->documentElement->firstChild;

/* A shallow import brings the attributes and leaves the children behind; a deep
 * one brings the subtree. Either way the copy is an orphan until it is linked. */
$dom_mimp_dst = Dom\XMLDocument::createFromString('<z/>');
$dom_mimp_say('shallow', static fn() => $dom_mimp_dst->importNode($dom_mimp_node));
$dom_mimp_dst->documentElement->appendChild($dom_mimp_dst->importNode($dom_mimp_node));
echo $dom_mimp_dst->saveXml(), "\n";
$dom_mimp_dst->documentElement->appendChild($dom_mimp_dst->importNode($dom_mimp_node, true));
echo $dom_mimp_dst->saveXml(), "\n";

/* A node already of this document is answered unchanged, and a DOCUMENT is the
 * code-9 refusal -- an exception on this tree, where the 2004 door warns. */
$dom_mimp_own = $dom_mimp_dst->documentElement;
var_dump($dom_mimp_dst->importNode($dom_mimp_own) === $dom_mimp_own);
$dom_mimp_say('a document', static fn() => $dom_mimp_dst->importNode($dom_mimp_src));
$dom_mimp_say('adopt a document', static fn() => $dom_mimp_dst->adoptNode($dom_mimp_src));

/* php declares the imported node nullable and then refuses a null, and the two
 * trees never mix: each door names the base class it takes. */
$dom_mimp_say('null', static fn() => $dom_mimp_dst->importNode(null));
$dom_mimp_say('an int', static fn() => $dom_mimp_dst->importNode(5));
$dom_mimp_say('adopt null', static fn() => $dom_mimp_dst->adoptNode(null));
$dom_mimp_legacy = new DOMDocument();
$dom_mimp_legacy->loadXML('<r xmlns:p="urn:p"><c p:a="1"><g/></c></r>');
$dom_mimp_say('a 2004 node', static fn() => $dom_mimp_dst->importNode($dom_mimp_legacy->documentElement));
$dom_mimp_say('a 2004 door handed this tree',
    static fn() => $dom_mimp_legacy->importNode($dom_mimp_src->documentElement));
$dom_mimp_say('the crossing handed this tree',
    static fn() => $dom_mimp_dst->importLegacyNode($dom_mimp_src->documentElement));

/* importLegacyNode is the one door a 2004 node crosses; the copy wears the
 * RECEIVER's family, so a DOMElement arrives as a Dom\Element. */
$dom_mimp_cross = Dom\XMLDocument::createFromString('<z/>');
$dom_mimp_say('crossing shallow',
    static fn() => $dom_mimp_cross->importLegacyNode($dom_mimp_legacy->documentElement->firstChild));
$dom_mimp_cross->documentElement->appendChild(
    $dom_mimp_cross->importLegacyNode($dom_mimp_legacy->documentElement->firstChild, true));
echo $dom_mimp_cross->saveXml(), "\n";
$dom_mimp_say('crossing a document', static fn() => $dom_mimp_cross->importLegacyNode($dom_mimp_legacy));

/* An imported namespaced ATTRIBUTE is re-pointed at a prefixed binding of the
 * same URI on the target's root. */
$dom_mimp_att = Dom\XMLDocument::createFromString('<z/>');
$dom_mimp_att->documentElement->setAttributeNodeNs(
    $dom_mimp_att->importNode($dom_mimp_node->attributes->item(0)));
echo $dom_mimp_att->saveXml(), "\n";

/* adoptNode MOVES: the same object comes back, its ownerDocument is the
 * receiver, and the source has lost it. (A FRAGMENT is adopted here too, where
 * the 2004 door refuses it with a bare false, but that case is not exercised
 * below: adopting one makes php 8.5.10 itself free an invalid pointer at
 * shutdown, so no expectation can be taken from it.) */
$dom_mimp_moved = Dom\XMLDocument::createFromString('<r><c/></r>');
$dom_mimp_mine = $dom_mimp_moved->documentElement->firstChild;
$dom_mimp_taken = $dom_mimp_att->adoptNode($dom_mimp_mine);
var_dump($dom_mimp_taken === $dom_mimp_mine, $dom_mimp_mine->ownerDocument === $dom_mimp_att);
$dom_mimp_att->documentElement->appendChild($dom_mimp_mine);
echo $dom_mimp_moved->saveXml(), $dom_mimp_att->saveXml(), "\n";
/* The three signatures php declares, which is where the nullable-then-refused
 * import shows, and their place in the declaration order. */
$dom_mimp_rc = new ReflectionClass('Dom\Document');
foreach (['importNode', 'adoptNode', 'importLegacyNode'] as $dom_mimp_name) {
    $dom_mimp_m = $dom_mimp_rc->getMethod($dom_mimp_name);
    $dom_mimp_ps = [];
    foreach ($dom_mimp_m->getParameters() as $dom_mimp_p) {
        $dom_mimp_t = $dom_mimp_p->getType();
        $dom_mimp_s = ($dom_mimp_t ? $dom_mimp_t . ' ' : '') . '$' . $dom_mimp_p->getName();
        if ($dom_mimp_p->isDefaultValueAvailable()) {
            $dom_mimp_s .= ' = ' . var_export($dom_mimp_p->getDefaultValue(), true);
        }
        $dom_mimp_ps[] = $dom_mimp_s;
    }
    echo $dom_mimp_name, '(', implode(', ', $dom_mimp_ps), '): ', $dom_mimp_m->getReturnType(), "\n";
}
?>
--EXPECT--
shallow => Dom\Element
<?xml version="1.0" encoding="UTF-8"?>
<z><c xmlns:p="urn:p" p:a="1"/></z>
<?xml version="1.0" encoding="UTF-8"?>
<z><c xmlns:p="urn:p" p:a="1"/><c xmlns:p="urn:p" p:a="1"><g/></c></z>
bool(true)
a document => DOMException(9): Not Supported Error
adopt a document => DOMException(9): Not Supported Error
null => TypeError(0): Dom\Document::importNode(): Argument #1 ($node) must be of type Dom\Node, null given
an int => TypeError(0): Dom\Document::importNode(): Argument #1 ($node) must be of type Dom\Node, int given
adopt null => TypeError(0): Dom\Document::adoptNode(): Argument #1 ($node) must be of type Dom\Node, null given
a 2004 node => TypeError(0): Dom\Document::importNode(): Argument #1 ($node) must be of type Dom\Node, DOMElement given
a 2004 door handed this tree => TypeError(0): DOMDocument::importNode(): Argument #1 ($node) must be of type DOMNode, Dom\Element given
the crossing handed this tree => TypeError(0): Dom\Document::importLegacyNode(): Argument #1 ($node) must be of type DOMNode, Dom\Element given
crossing shallow => Dom\Element
<?xml version="1.0" encoding="UTF-8"?>
<z><c xmlns:p="urn:p" p:a="1"><g/></c></z>
crossing a document => DOMException(9): Not Supported Error
<?xml version="1.0" encoding="UTF-8"?>
<z xmlns:p="urn:p" p:a="1"/>
bool(true)
bool(true)
<?xml version="1.0" encoding="UTF-8"?>
<r/><?xml version="1.0" encoding="UTF-8"?>
<z xmlns:p="urn:p" p:a="1"><c/></z>
importNode(?Dom\Node $node, bool $deep = false): Dom\Node
adoptNode(Dom\Node $node): Dom\Node
importLegacyNode(DOMNode $node, bool $deep = false): Dom\Node
