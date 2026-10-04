--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\Element::insertAdjacentElement/insertAdjacentText: the enum position, adoption, the document sentences
--FILE--
<?php
use Dom\AdjacentPosition as DomMiaP;
$dom_mia_try = function ($l, $f) { echo "== $l\n"; try { $r = $f(); echo "  ret: ", var_export($r, true), "\n"; } catch (Throwable $e) { echo "  ", get_class($e), "(", $e->getCode(), "): ", $e->getMessage(), "\n"; } };
$dom_mia_pos = [DomMiaP::BeforeBegin, DomMiaP::AfterBegin, DomMiaP::BeforeEnd, DomMiaP::AfterEnd];
foreach ($dom_mia_pos as $dom_mia_p) {
  $dom_mia_try('element '.$dom_mia_p->name, function() use ($dom_mia_p) {
    $d = Dom\XMLDocument::createFromString('<r>pre<a>t<k/>u</a>post<b/></r>');
    $a = $d->documentElement->firstElementChild; $n = $d->createElement('n');
    $ret = $a->insertAdjacentElement($dom_mia_p, $n);
    return [$ret === $n, get_class($ret), $d->saveXml($d->documentElement)];
  });
}
foreach ($dom_mia_pos as $dom_mia_p) {
  $dom_mia_try('text '.$dom_mia_p->name, function() use ($dom_mia_p) {
    $d = Dom\XMLDocument::createFromString('<r>pre<a>t<k/>u</a>post<b/></r>');
    $a = $d->documentElement->firstElementChild;
    return [$a->insertAdjacentText($dom_mia_p, 'X&<Y'), $d->saveXml($d->documentElement)];
  });
}
foreach ($dom_mia_pos as $dom_mia_p) {
  $dom_mia_try('root '.$dom_mia_p->name, function() use ($dom_mia_p) {
    $d = Dom\XMLDocument::createFromString('<r><a/></r>');
    $ret = $d->documentElement->insertAdjacentElement($dom_mia_p, $d->createElement('n'));
    return [$ret === null ? null : $ret->tagName, trim($d->saveXml())];
  });
  $dom_mia_try('root text '.$dom_mia_p->name, function() use ($dom_mia_p) {
    $d = Dom\XMLDocument::createFromString('<r><a/></r>');
    $d->documentElement->insertAdjacentText($dom_mia_p, 'q');
    return trim($d->saveXml());
  });
}
foreach ($dom_mia_pos as $dom_mia_p) {
  $dom_mia_try('detached '.$dom_mia_p->name, function() use ($dom_mia_p) {
    $d = Dom\XMLDocument::createFromString('<r/>');
    $a = $d->createElement('a');
    $ret = $a->insertAdjacentElement($dom_mia_p, $d->createElement('n'));
    $a->insertAdjacentText($dom_mia_p, 'q');
    return [$ret === null ? null : $ret->tagName, $d->saveXml($a)];
  });
}
$dom_mia_try('an ancestor into its own descendant', function() {
  $d = Dom\XMLDocument::createFromString('<r><a><k/></a></r>');
  $k = $d->getElementsByTagName('k')->item(0);
  return $k->insertAdjacentElement(DomMiaP::BeforeEnd, $d->documentElement->firstElementChild);
});
$dom_mia_try('a node of ANOTHER document is adopted, not refused', function() {
  $d1 = Dom\XMLDocument::createFromString('<r><a/></r>');
  $d2 = Dom\XMLDocument::createFromString('<s><z/></s>');
  $z = $d2->documentElement->firstElementChild;
  $ret = $d1->documentElement->firstElementChild->insertAdjacentElement(DomMiaP::AfterEnd, $z);
  return [$ret === $z, get_class($ret), $d1->saveXml($d1->documentElement), $d2->saveXml($d2->documentElement)];
});
$dom_mia_try('an EMPTY chunk still makes a text child', function() {
  $d = Dom\XMLDocument::createFromString('<r><a/></r>');
  $a = $d->documentElement->firstElementChild;
  $a->insertAdjacentText(DomMiaP::AfterBegin, '');
  return [$d->saveXml($d->documentElement), get_class($a->firstChild)];
});
$dom_mia_try('the new text does NOT merge with the sibling it lands beside', function() {
  $d = Dom\XMLDocument::createFromString('<r>pre<a/>post</r>');
  $d->documentElement->firstElementChild->insertAdjacentText(DomMiaP::BeforeBegin, 'Z');
  $o = [];
  foreach ($d->documentElement->childNodes as $c) { $o[] = get_class($c).' '.var_export($c->nodeValue, true); }
  return $o;
});
$dom_mia_try('the position is the ENUM, not a word', function() {
  $d = Dom\XMLDocument::createFromString('<r><a/></r>');
  return $d->documentElement->firstElementChild->insertAdjacentElement('beforeend', $d->createElement('n'));
});
$dom_mia_try('and the element is the MODERN one', function() {
  $d = Dom\XMLDocument::createFromString('<r><a/></r>');
  $o = new DOMDocument; $o->loadXML('<o><c/></o>');
  return $d->documentElement->firstElementChild->insertAdjacentElement(DomMiaP::BeforeEnd, $o->documentElement->firstChild);
});
$dom_mia_try('the 2004 door keeps its word and its own refusal', function() {
  $d = new DOMDocument; $d->loadXML('<r><a/></r>');
  $a = $d->documentElement->firstChild;
  $ret = $a->insertAdjacentElement('AfterEnd', $d->createElement('n'));
  $a->insertAdjacentElement('nope', $d->createElement('m'));
  return [$ret->tagName, $d->saveXML($d->documentElement)];
});
$dom_mia_try('...and takes a second element child of a document without a word', function() {
  $d = new DOMDocument; $d->loadXML('<r/>');
  $ret = $d->documentElement->insertAdjacentElement('beforebegin', $d->createElement('n'));
  $d->documentElement->insertAdjacentText('afterend', 'q');
  return [$ret->tagName, trim($d->saveXML())];
});
--EXPECT--
== element BeforeBegin
  ret: array (
  0 => true,
  1 => 'Dom\\Element',
  2 => '<r>pre<n/><a>t<k/>u</a>post<b/></r>',
)
== element AfterBegin
  ret: array (
  0 => true,
  1 => 'Dom\\Element',
  2 => '<r>pre<a><n/>t<k/>u</a>post<b/></r>',
)
== element BeforeEnd
  ret: array (
  0 => true,
  1 => 'Dom\\Element',
  2 => '<r>pre<a>t<k/>u<n/></a>post<b/></r>',
)
== element AfterEnd
  ret: array (
  0 => true,
  1 => 'Dom\\Element',
  2 => '<r>pre<a>t<k/>u</a><n/>post<b/></r>',
)
== text BeforeBegin
  ret: array (
  0 => NULL,
  1 => '<r>preX&amp;&lt;Y<a>t<k/>u</a>post<b/></r>',
)
== text AfterBegin
  ret: array (
  0 => NULL,
  1 => '<r>pre<a>X&amp;&lt;Yt<k/>u</a>post<b/></r>',
)
== text BeforeEnd
  ret: array (
  0 => NULL,
  1 => '<r>pre<a>t<k/>uX&amp;&lt;Y</a>post<b/></r>',
)
== text AfterEnd
  ret: array (
  0 => NULL,
  1 => '<r>pre<a>t<k/>u</a>X&amp;&lt;Ypost<b/></r>',
)
== root BeforeBegin
  DOMException(3): Cannot have more than one element child in a document
== root text BeforeBegin
  DOMException(3): Cannot insert text as a child of a document
== root AfterBegin
  ret: array (
  0 => 'n',
  1 => '<?xml version="1.0" encoding="UTF-8"?>
<r><n/><a/></r>',
)
== root text AfterBegin
  ret: '<?xml version="1.0" encoding="UTF-8"?>
<r>q<a/></r>'
== root BeforeEnd
  ret: array (
  0 => 'n',
  1 => '<?xml version="1.0" encoding="UTF-8"?>
<r><a/><n/></r>',
)
== root text BeforeEnd
  ret: '<?xml version="1.0" encoding="UTF-8"?>
<r><a/>q</r>'
== root AfterEnd
  DOMException(3): Cannot have more than one element child in a document
== root text AfterEnd
  DOMException(3): Cannot insert text as a child of a document
== detached BeforeBegin
  ret: array (
  0 => NULL,
  1 => '<a/>',
)
== detached AfterBegin
  ret: array (
  0 => 'n',
  1 => '<a>q<n/></a>',
)
== detached BeforeEnd
  ret: array (
  0 => 'n',
  1 => '<a><n/>q</a>',
)
== detached AfterEnd
  ret: array (
  0 => NULL,
  1 => '<a/>',
)
== an ancestor into its own descendant
  DOMException(3): Hierarchy Request Error
== a node of ANOTHER document is adopted, not refused
  ret: array (
  0 => true,
  1 => 'Dom\\Element',
  2 => '<r><a/><z/></r>',
  3 => '<s/>',
)
== an EMPTY chunk still makes a text child
  ret: array (
  0 => '<r><a></a></r>',
  1 => 'Dom\\Text',
)
== the new text does NOT merge with the sibling it lands beside
  ret: array (
  0 => 'Dom\\Text \'pre\'',
  1 => 'Dom\\Text \'Z\'',
  2 => 'Dom\\Element NULL',
  3 => 'Dom\\Text \'post\'',
)
== the position is the ENUM, not a word
  TypeError(0): Dom\Element::insertAdjacentElement(): Argument #1 ($where) must be of type Dom\AdjacentPosition, string given
== and the element is the MODERN one
  TypeError(0): Dom\Element::insertAdjacentElement(): Argument #2 ($element) must be of type Dom\Element, DOMElement given
== the 2004 door keeps its word and its own refusal
  DOMException(12): Syntax Error
== ...and takes a second element child of a document without a word
  ret: array (
  0 => 'n',
  1 => '<?xml version="1.0"?>
<n/>
q
<r/>',
)
