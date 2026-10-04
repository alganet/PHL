--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\Element::$innerHTML/$outerHTML read: saveXml's bytes, no formatOutput, the well-formed screen
--FILE--
<?php
$dom_ioh_try = function ($l, $f) { echo "== $l\n"; try { echo "  ", var_export($f(), true), "\n"; } catch (Throwable $e) { echo "  ", get_class($e), "(", $e->getCode(), "): ", $e->getMessage(), "\n"; } };
$dom_ioh_doc = function ($x) { return Dom\XMLDocument::createFromString($x); };

$dom_ioh_try('inner, namespaced', function () use ($dom_ioh_doc) {
  $d = $dom_ioh_doc('<r xmlns="urn:d" xmlns:a="urn:a"><c a:k="1">text<e/></c></r>');
  return $d->documentElement->firstElementChild->innerHTML;
});
$dom_ioh_try('outer, namespaced', function () use ($dom_ioh_doc) {
  $d = $dom_ioh_doc('<r xmlns="urn:d" xmlns:a="urn:a"><c a:k="1">text<e/></c></r>');
  return $d->documentElement->firstElementChild->outerHTML;
});
$dom_ioh_try('outer, prefixed child mints the binding', function () use ($dom_ioh_doc) {
  $d = $dom_ioh_doc('<r xmlns:a="urn:a"><a:c><a:x/></a:c></r>');
  return $d->documentElement->firstElementChild->firstElementChild->outerHTML;
});
$dom_ioh_try('inner, empty element', function () use ($dom_ioh_doc) {
  return $dom_ioh_doc('<r><c/></r>')->documentElement->firstElementChild->innerHTML;
});
$dom_ioh_try('inner, comment + PI + CDATA', function () use ($dom_ioh_doc) {
  $d = $dom_ioh_doc('<r><c><!--k--><?pi v?><![CDATA[<raw>]]></c></r>');
  return $d->documentElement->firstElementChild->innerHTML;
});
$dom_ioh_try('inner, text is escaped', function () use ($dom_ioh_doc) {
  $d = $dom_ioh_doc('<r><c>a &lt; b &amp; "c"</c></r>');
  return $d->documentElement->firstElementChild->innerHTML;
});
/* formatOutput reaches saveXml($node) and NOT these two. */
$dom_ioh_try('formatOutput is not read', function () use ($dom_ioh_doc) {
  $d = $dom_ioh_doc('<r><c><a/><b/></c></r>');
  $d->formatOutput = true;
  $c = $d->documentElement->firstElementChild;
  return [$c->innerHTML, $c->outerHTML, $d->saveXml($c)];
});
/* The require-well-formed flag saveXml runs without: a comment holding "--" or
 * ending in "-" writes out fine there and is a refusal here. */
foreach (['a--b', 'a-', 'a b'] as $dom_ioh_c) {
  $dom_ioh_try('comment '.var_export($dom_ioh_c, true), function () use ($dom_ioh_c) {
    $d = Dom\XMLDocument::createEmpty();
    $r = $d->appendChild($d->createElement('r'));
    $r->appendChild($d->createComment($dom_ioh_c));
    return [$d->saveXml($r), $r->innerHTML];
  });
}
$dom_ioh_try('processing instruction targeting the declaration', function () {
  $d = Dom\XMLDocument::createEmpty();
  $r = $d->appendChild($d->createElement('r'));
  $r->appendChild($d->createProcessingInstruction('XmL', 'v'));
  return [$d->saveXml($r), $r->innerHTML];
});
$dom_ioh_try('text outside the character production', function () {
  $d = Dom\XMLDocument::createEmpty();
  $r = $d->appendChild($d->createElement('r'));
  $r->appendChild($d->createTextNode("a\x01b"));
  return $r->innerHTML;
});
$dom_ioh_try('a CDATA section has no screen', function () {
  $d = Dom\XMLDocument::createEmpty();
  $r = $d->appendChild($d->createElement('r'));
  $r->appendChild($d->createCDATASection('a--b'));
  return $r->innerHTML;
});
/* The 2004 tree declares neither name. */
$dom_ioh_d2 = new DOMDocument();
$dom_ioh_d2->loadXML('<r><c/></r>');
var_dump(property_exists('DOMElement', 'innerHTML'), property_exists('Dom\Element', 'innerHTML'));
--EXPECT--
== inner, namespaced
  'text<e xmlns="urn:d"/>'
== outer, namespaced
  '<c xmlns="urn:d" xmlns:a="urn:a" a:k="1">text<e/></c>'
== outer, prefixed child mints the binding
  '<a:x xmlns:a="urn:a"/>'
== inner, empty element
  ''
== inner, comment + PI + CDATA
  '<!--k--><?pi v?><![CDATA[<raw>]]>'
== inner, text is escaped
  'a &lt; b &amp; "c"'
== formatOutput is not read
  array (
  0 => '<a/><b/>',
  1 => '<c><a/><b/></c>',
  2 => '<c>
  <a/>
  <b/>
</c>',
)
== comment 'a--b'
    DOMException(12): The resulting XML serialization is not well-formed
== comment 'a-'
    DOMException(12): The resulting XML serialization is not well-formed
== comment 'a b'
  array (
  0 => '<r><!--a b--></r>',
  1 => '<!--a b-->',
)
== processing instruction targeting the declaration
    DOMException(12): The resulting XML serialization is not well-formed
== text outside the character production
    DOMException(12): The resulting XML serialization is not well-formed
== a CDATA section has no screen
  '<![CDATA[a--b]]>'
bool(false)
bool(true)
