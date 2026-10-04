--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A document holding an entity reference hung every element search and node serializer
--FILE--
<?php
/* An entity reference's children are the ENTITY DECLARATION, which lives in the
 * internal subset and is not below the reference at all; the DTD node's own
 * children are every declaration in that subset. Neither is document content,
 * and a document-order walk that descends into either leaves the subtree it was
 * given -- from the subset it comes back out at the document element, which it
 * has already visited, and walks the same ring for ever.
 *
 * Every element search and the namespace pass the node serializer runs share
 * one such walk, so a document with an internal subset -- which is any document
 * that can HAVE an entity reference -- hung all of them. Both trees are
 * affected, and neither needs the reference to be written: parsing one is
 * enough.
 *
 * php sees neither declaration set from these doors: an element search never
 * looks inside an entity's replacement content, and the serializer writes a
 * reference by name rather than by content. */

$xml = '<!DOCTYPE r [<!ENTITY e "<k>in</k>">]><r><a>x&e;y</a><k>real</k></r>';

foreach (['DOMDocument', 'Dom\XMLDocument'] as $cls) {
    if ($cls === 'DOMDocument') {
        $d = new DOMDocument();
        $d->loadXML($xml);
    } else {
        $d = Dom\XMLDocument::createFromString($xml);
    }
    echo "== ", $cls, "\n";

    /* The search reaches the real element and NOT the one inside the entity's
     * replacement content, which is reachable only through `doctype`. */
    $found = [];
    foreach ($d->getElementsByTagName('k') as $n) {
        $found[] = $n->textContent;
    }
    echo "  getElementsByTagName: ", count($found), " [", implode(',', $found), "]\n";

    /* ...and the node serializer writes the reference by name. */
    $a = $d->documentElement->firstElementChild;
    echo "  saveXml(node): ", $d->saveXML($a), "\n";
    echo "  textContent:   ", $a->textContent, "\n";

    /* The declaration is still there, through the door that owns it. */
    echo "  doctype entity: ", $d->doctype->entities->item(0)->nodeName, "\n";
}
?>
--EXPECT--
== DOMDocument
  getElementsByTagName: 1 [real]
  saveXml(node): <a>x&e;y</a>
  textContent:   xiny
  doctype entity: e
== Dom\XMLDocument
  getElementsByTagName: 1 [real]
  saveXml(node): <a>x&e;y</a>
  textContent:   xiny
  doctype entity: e
