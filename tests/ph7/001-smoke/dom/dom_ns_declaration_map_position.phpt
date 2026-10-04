--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\Element: a WRITTEN namespace declaration is listed where it was written
--FILE--
<?php
const NSMP_XMLNS = 'http://www.w3.org/2000/xmlns/';
/* The three faces of one order: the iterated map, the map by index, and the
 * sibling chain walked forwards then backwards. */
function nsmp_name($a) { return ($a->prefix ? $a->prefix . ':' : '') . $a->localName; }
function nsmp_row($zLabel, $e) {
	$aIter = [];
	foreach ($e->attributes as $a) { $aIter[] = nsmp_name($a); }
	$aAt = [];
	for ($i = 0; $i < $e->attributes->length; $i++) { $aAt[] = nsmp_name($e->attributes->item($i)); }
	$aFwd = [];
	for ($a = $e->attributes->item(0); $a; $a = $a->nextSibling) { $aFwd[] = nsmp_name($a); }
	$aBack = [];
	for ($a = $e->attributes->item($e->attributes->length - 1); $a; $a = $a->previousSibling) {
		$aBack[] = nsmp_name($a);
	}
	echo str_pad($zLabel, 26), ' [', implode(' ', $aIter), ']';
	echo ' at[', implode(' ', $aAt), ']';
	echo ' fwd[', implode(' ', $aFwd), ']';
	echo ' back[', implode(' ', array_reverse($aBack)), "]\n";
}
/* Parsed: the declarations come first whatever the source order. */
$d = Dom\XMLDocument::createFromString('<r xmlns:p="urn:p" a="1"/>');
nsmp_row('parsed first', $d->documentElement);
$d = Dom\XMLDocument::createFromString('<r a="1" xmlns:p="urn:p" b="2"/>');
nsmp_row('parsed middle', $d->documentElement);
/* Written: after the attributes the element had, before the ones written next. */
$d = Dom\XMLDocument::createFromString('<r a="1"/>');
$e = $d->documentElement;
$e->setAttributeNS(NSMP_XMLNS, 'xmlns:w', 'urn:w');
nsmp_row('written', $e);
$e->setAttribute('b', '2');
nsmp_row('then an attribute', $e);
$e->setAttributeNS(NSMP_XMLNS, 'xmlns:z', 'urn:z');
nsmp_row('then a declaration', $e);
$e->setAttributeNS(NSMP_XMLNS, 'xmlns:w', 'urn:w2');
nsmp_row('rebound in place', $e);
/* With no attributes yet, a written one is the head -- where the parser's are. */
$d = Dom\XMLDocument::createFromString('<r/>');
$e = $d->documentElement;
$e->setAttributeNS(NSMP_XMLNS, 'xmlns:w', 'urn:w');
$e->setAttribute('a', '1');
nsmp_row('written onto none', $e);
$d = Dom\XMLDocument::createFromString('<r xmlns:p="urn:p" a="1"/>');
$e = $d->documentElement;
$e->setAttributeNS(NSMP_XMLNS, 'xmlns:w', 'urn:w');
nsmp_row('written onto parsed', $e);
/* The attribute a declaration follows can leave: the chain closes over it. */
$d = Dom\XMLDocument::createFromString('<r a="1" b="2"/>');
$e = $d->documentElement;
$e->setAttributeNS(NSMP_XMLNS, 'xmlns:w', 'urn:w');
$e->removeAttribute('b');
nsmp_row('anchor removed', $e);
$e->removeAttribute('a');
nsmp_row('all removed', $e);
$e->setAttribute('c', '3');
nsmp_row('then a new one', $e);
$d = Dom\XMLDocument::createFromString('<r a="1" b="2"/>');
$e = $d->documentElement;
$e->setAttributeNS(NSMP_XMLNS, 'xmlns:w', 'urn:w');
$e->removeAttributeNode($e->getAttributeNode('b'));
nsmp_row('anchor node removed', $e);
/* A copy carries the position -- and lists no binding the engine minted. */
$d = Dom\XMLDocument::createFromString('<r a="1"/>');
$e = $d->documentElement;
$e->setAttributeNS(NSMP_XMLNS, 'xmlns:w', 'urn:w');
nsmp_row('clone', $e->cloneNode(true));
nsmp_row('document clone', $d->cloneNode(true)->documentElement);
$d = Dom\XMLDocument::createFromString('<r xmlns:p="urn:p"><p:kid a="1"/></r>');
nsmp_row('clone of an heir', $d->documentElement->firstElementChild->cloneNode(true));
/* An xmlns attribute NODE, and the toggle, are written the same way. */
$d = Dom\XMLDocument::createFromString('<r a="1"/>');
$e = $d->documentElement;
$at = $d->createAttributeNS(NSMP_XMLNS, 'xmlns:z');
$at->value = 'urn:z';
$e->setAttributeNodeNS($at);
nsmp_row('an xmlns node', $e);
$d = Dom\XMLDocument::createFromString('<r a="1"/>');
$e = $d->documentElement;
$e->toggleAttribute('xmlns:t');
nsmp_row('toggled on', $e);
/* The 2004 tree lists no declaration at all, however it is written. */
$l = new DOMDocument();
$l->loadXML('<r xmlns:p="urn:p" a="1"/>');
nsmp_row('2004 parsed', $l->documentElement);
$l = new DOMDocument();
$l->loadXML('<r a="1"/>');
$l->documentElement->setAttributeNS(NSMP_XMLNS, 'xmlns:w', 'urn:w');
nsmp_row('2004 written', $l->documentElement);
?>
--EXPECT--
parsed first               [xmlns:p a] at[xmlns:p a] fwd[xmlns:p a] back[xmlns:p a]
parsed middle              [xmlns:p a b] at[xmlns:p a b] fwd[xmlns:p a b] back[xmlns:p a b]
written                    [a xmlns:w] at[a xmlns:w] fwd[a xmlns:w] back[a xmlns:w]
then an attribute          [a xmlns:w b] at[a xmlns:w b] fwd[a xmlns:w b] back[a xmlns:w b]
then a declaration         [a xmlns:w b xmlns:z] at[a xmlns:w b xmlns:z] fwd[a xmlns:w b xmlns:z] back[a xmlns:w b xmlns:z]
rebound in place           [a xmlns:w b xmlns:z] at[a xmlns:w b xmlns:z] fwd[a xmlns:w b xmlns:z] back[a xmlns:w b xmlns:z]
written onto none          [xmlns:w a] at[xmlns:w a] fwd[xmlns:w a] back[xmlns:w a]
written onto parsed        [xmlns:p a xmlns:w] at[xmlns:p a xmlns:w] fwd[xmlns:p a xmlns:w] back[xmlns:p a xmlns:w]
anchor removed             [a xmlns:w] at[a xmlns:w] fwd[a xmlns:w] back[a xmlns:w]
all removed                [xmlns:w] at[xmlns:w] fwd[xmlns:w] back[xmlns:w]
then a new one             [xmlns:w c] at[xmlns:w c] fwd[xmlns:w c] back[xmlns:w c]
anchor node removed        [a xmlns:w] at[a xmlns:w] fwd[a xmlns:w] back[a xmlns:w]
clone                      [a xmlns:w] at[a xmlns:w] fwd[a xmlns:w] back[a xmlns:w]
document clone             [a xmlns:w] at[a xmlns:w] fwd[a xmlns:w] back[a xmlns:w]
clone of an heir           [a] at[a] fwd[a] back[a]
an xmlns node              [a xmlns:z] at[a xmlns:z] fwd[a xmlns:z] back[a xmlns:z]
toggled on                 [a xmlns:t] at[a xmlns:t] fwd[a xmlns:t] back[a xmlns:t]
2004 parsed                [a] at[a] fwd[a] back[a]
2004 written               [a] at[a] fwd[a] back[a]
