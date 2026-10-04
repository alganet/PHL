--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\Element: the XML fragment parser behind innerHTML/outerHTML writes and insertAdjacentHTML
--FILE--
<?php
use Dom\AdjacentPosition as DomFrgP;
$dom_frg_try = function ($l, $f) { echo "== $l\n"; try { echo "  ", var_export($f(), true), "\n"; } catch (Throwable $e) { echo "  ", get_class($e), "(", $e->getCode(), "): ", $e->getMessage(), "\n"; } };
$dom_frg_c = function ($x = '<r xmlns="urn:d" xmlns:a="urn:a"><c>t</c></r>') {
  $d = Dom\XMLDocument::createFromString($x);
  return [$d, $d->documentElement->firstElementChild];
};
/* The chunk is parsed under a synthetic root carrying the CONTEXT node's own
 * tag name and every namespace in scope where it sits. */
$dom_frg_try('inner: a bare name inherits the default binding', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->innerHTML = '<x/>';
  return [$c->firstChild->namespaceURI, $d->saveXml($d->documentElement)];
});
$dom_frg_try('inner: an ancestor prefix resolves', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->innerHTML = '<a:x/>';
  return [$c->firstChild->namespaceURI, $d->saveXml($d->documentElement)];
});
$dom_frg_try('inner: a prefix bound nowhere is a refusal', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->innerHTML = '<zz:x/>'; return $d->saveXml($d->documentElement);
});
$dom_frg_try('inner: a new binding is kept', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c('<r><c/></r>'); $c->innerHTML = '<x xmlns="urn:z"/>';
  return [$c->innerHTML, $d->saveXml($d->documentElement)];
});
$dom_frg_try('inner: several roots and bare text are a fragment', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->innerHTML = 'hi &amp; <x/><y/>';
  return $d->saveXml($d->documentElement);
});
$dom_frg_try('inner: comment, PI and CDATA travel', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->innerHTML = '<!--k--><?pi v?><![CDATA[z]]>';
  return [$c->innerHTML, $d->saveXml($d->documentElement)];
});
$dom_frg_try('inner: an unclosed tag is a refusal', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->innerHTML = '<x>'; return $d->saveXml($d->documentElement);
});
$dom_frg_try('inner: an undeclared entity is a refusal', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->innerHTML = '&nope;'; return $d->saveXml($d->documentElement);
});
$dom_frg_try('inner: the empty chunk clears the children', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c('<r xmlns="urn:d"><c>old<z/></c></r>'); $c->innerHTML = '';
  return [$c->innerHTML, $d->saveXml($d->documentElement)];
});
$dom_frg_try('outer: the node is replaced', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->outerHTML = '<y/>z';
  return $d->saveXml($d->documentElement);
});
$dom_frg_try('outer: a receiver the document owns is refused', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $d->documentElement->outerHTML = '<n/>';
  return $d->saveXml($d->documentElement);
});
$dom_frg_try('outer: a parentless receiver returns before parsing', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $e = $d->createElement('q'); $e->appendChild($d->createElement('in'));
  $e->outerHTML = '<x>';
  return $d->saveXml($e);
});
/* insertAdjacentHTML is the third door: it parses where its two siblings adopt. */
foreach ([DomFrgP::BeforeBegin, DomFrgP::AfterBegin, DomFrgP::BeforeEnd, DomFrgP::AfterEnd] as $dom_frg_p) {
  $dom_frg_try('insertAdjacentHTML '.$dom_frg_p->name, function () use ($dom_frg_c, $dom_frg_p) {
    [$d, $c] = $dom_frg_c(); $c->insertAdjacentHTML($dom_frg_p, '<x/>y');
    return $d->saveXml($d->documentElement);
  });
}
foreach ([DomFrgP::BeforeBegin, DomFrgP::AfterEnd] as $dom_frg_p) {
  $dom_frg_try('insertAdjacentHTML '.$dom_frg_p->name.' on the root', function () use ($dom_frg_c, $dom_frg_p) {
    [$d, $c] = $dom_frg_c(); $d->documentElement->insertAdjacentHTML($dom_frg_p, '<x/>');
    return $d->saveXml($d->documentElement);
  });
  $dom_frg_try('insertAdjacentHTML '.$dom_frg_p->name.' on a parentless element', function () use ($dom_frg_c, $dom_frg_p) {
    [$d, $c] = $dom_frg_c(); $d->createElement('q')->insertAdjacentHTML($dom_frg_p, '<x>');
    return $d->saveXml($d->documentElement);
  });
}
$dom_frg_try('insertAdjacentHTML AfterBegin on a parentless element', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $e = $d->createElement('q');
  $e->insertAdjacentHTML(DomFrgP::AfterBegin, '<x/>');
  return $d->saveXml($e);
});
$dom_frg_try('insertAdjacentHTML: the empty chunk moves nothing', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->insertAdjacentHTML(DomFrgP::AfterBegin, '');
  return $d->saveXml($d->documentElement);
});
$dom_frg_try('insertAdjacentHTML: a malformed chunk is the Syntax refusal', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->insertAdjacentHTML(DomFrgP::AfterBegin, '<x>');
  return $d->saveXml($d->documentElement);
});
$dom_frg_try('insertAdjacentHTML: the word is the enum, not a string', function () use ($dom_frg_c) {
  [$d, $c] = $dom_frg_c(); $c->insertAdjacentHTML('afterbegin', '<x/>');
  return $d->saveXml($d->documentElement);
});
--EXPECT--
== inner: a bare name inherits the default binding
  array (
  0 => 'urn:d',
  1 => '<r xmlns="urn:d" xmlns:a="urn:a"><c><x/></c></r>',
)
== inner: an ancestor prefix resolves
  array (
  0 => 'urn:a',
  1 => '<r xmlns="urn:d" xmlns:a="urn:a"><c><a:x/></c></r>',
)
== inner: a prefix bound nowhere is a refusal
    DOMException(12): XML fragment is not well-formed
== inner: a new binding is kept
  array (
  0 => '<x xmlns="urn:z"/>',
  1 => '<r><c><x xmlns="urn:z"/></c></r>',
)
== inner: several roots and bare text are a fragment
  '<r xmlns="urn:d" xmlns:a="urn:a"><c>hi &amp; <x/><y/></c></r>'
== inner: comment, PI and CDATA travel
  array (
  0 => '<!--k--><?pi v?><![CDATA[z]]>',
  1 => '<r xmlns="urn:d" xmlns:a="urn:a"><c><!--k--><?pi v?><![CDATA[z]]></c></r>',
)
== inner: an unclosed tag is a refusal
    DOMException(12): XML fragment is not well-formed
== inner: an undeclared entity is a refusal
    DOMException(12): XML fragment is not well-formed
== inner: the empty chunk clears the children
  array (
  0 => '',
  1 => '<r xmlns="urn:d"><c/></r>',
)
== outer: the node is replaced
  '<r xmlns="urn:d" xmlns:a="urn:a"><y/>z</r>'
== outer: a receiver the document owns is refused
    DOMException(13): Invalid Modification Error
== outer: a parentless receiver returns before parsing
  '<q><in/></q>'
== insertAdjacentHTML BeforeBegin
  '<r xmlns="urn:d" xmlns:a="urn:a"><x/>y<c>t</c></r>'
== insertAdjacentHTML AfterBegin
  '<r xmlns="urn:d" xmlns:a="urn:a"><c><x/>yt</c></r>'
== insertAdjacentHTML BeforeEnd
  '<r xmlns="urn:d" xmlns:a="urn:a"><c>t<x/>y</c></r>'
== insertAdjacentHTML AfterEnd
  '<r xmlns="urn:d" xmlns:a="urn:a"><c>t</c><x/>y</r>'
== insertAdjacentHTML BeforeBegin on the root
    DOMException(7): No Modification Allowed Error
== insertAdjacentHTML BeforeBegin on a parentless element
    DOMException(7): No Modification Allowed Error
== insertAdjacentHTML AfterEnd on the root
    DOMException(7): No Modification Allowed Error
== insertAdjacentHTML AfterEnd on a parentless element
    DOMException(7): No Modification Allowed Error
== insertAdjacentHTML AfterBegin on a parentless element
  '<q><x/></q>'
== insertAdjacentHTML: the empty chunk moves nothing
  '<r xmlns="urn:d" xmlns:a="urn:a"><c>t</c></r>'
== insertAdjacentHTML: a malformed chunk is the Syntax refusal
    DOMException(12): XML fragment is not well-formed
== insertAdjacentHTML: the word is the enum, not a string
    TypeError(0): Dom\Element::insertAdjacentHTML(): Argument #1 ($where) must be of type Dom\AdjacentPosition, string given
