--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
new DOMProcessingInstruction / DOMDocumentFragment / DOMEntityReference: the last three constructors, the fragment's split personality, and the reference that stays resolved
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "W[$no]: $str\n"; return true; });
function cf($label, $fn) {
    try { $r = $fn(); echo "$label: ", is_object($r) ? get_class($r) : var_export($r, true), "\n"; }
    catch (Throwable $e) { echo "$label: ", get_class($e), '(', $e->getCode(), ') ', $e->getMessage(), "\n"; }
}

/* the PI validates its target as a plain Name -- xml and a prefixed spelling
 * both pass -- and stores its data literally, NULL when omitted */
foreach (['', '1bad', 'a b'] as $n) { cf("pi '$n'", fn() => new DOMProcessingInstruction($n)); }
foreach (['xml', 'XML', 'xml-stylesheet', 'p:a'] as $n) { cf("pi '$n'", fn() => (new DOMProcessingInstruction($n))->target); }
cf('pi omitted data', function() { $p = new DOMProcessingInstruction('t'); return [$p->nodeValue, $p->data, $p->nodeName]; });
cf('pi data literal', fn() => (new DOMProcessingInstruction('t', 'a?>b'))->data);
cf('pi write before adoption', function() { $p = new DOMProcessingInstruction('t', 'd'); $p->data = 'w'; return $p->data; });

/* the fragment: readable, append()-able, and refusing the level-2 doors the
 * ownerless way -- appendXML included */
cf('frag shape', function() { $f = new DOMDocumentFragment; return [$f->nodeName, $f->nodeType, $f->ownerDocument, $f->childNodes->length]; });
cf('frag append works', function() { $f = new DOMDocumentFragment; $f->append(new DOMElement('a'), 'txt'); return [$f->childNodes->length, $f->textContent]; });
cf('frag appendChild refused', fn() => (new DOMDocumentFragment)->appendChild(new DOMText('t')));
cf('frag appendXML refused', fn() => (new DOMDocumentFragment)->appendXML('<a/>'));
cf('built fragment splices whole', function() {
    $d = new DOMDocument; $d->loadXML('<r/>');
    $f = new DOMDocumentFragment;
    $f->append(new DOMElement('k', 'v'), 'txt');
    $kid = $f->firstChild;
    $d->documentElement->appendChild($f);
    return [$d->saveXML($d->documentElement), $kid->ownerDocument === $d, $f->ownerDocument === $d, $f->childNodes->length];
});

/* the entity reference: name-checked, resolved against nothing until an
 * insertion gives it a document -- and php KEEPS it resolved through the
 * adoption, though the raw child link the constructor made does not survive
 * it (replaceChild's childless silence measures that) */
foreach (['', '1bad', 'a b'] as $n) { cf("entref '$n'", fn() => new DOMEntityReference($n)); }
cf('entref predefined resolves', function() { $r = new DOMEntityReference('amp'); return [$r->nodeName, $r->childNodes->length, $r->firstChild->nodeType, $r->firstChild === $r->firstChild]; });
cf('entref unknown is empty', function() { $r = new DOMEntityReference('nosuch'); return [$r->firstChild, $r->childNodes->length]; });
cf('adopted entref serializes', function() {
    $d = new DOMDocument; $d->loadXML('<r/>');
    $d->documentElement->appendChild(new DOMEntityReference('amp'));
    $d->documentElement->appendChild(new DOMEntityReference('nosuch'));
    return $d->saveXML($d->documentElement);
});
cf('adopted entref reader still resolves', function() {
    $d = new DOMDocument; $d->loadXML('<r/>');
    $r = new DOMEntityReference('lt');
    $d->documentElement->appendChild($r);
    return [$r->childNodes->length, $r->firstChild->nodeType];
});
cf('adopted entref raw link is gone', function() {
    $d = new DOMDocument; $d->loadXML('<r/>');
    $r = new DOMEntityReference('amp');
    $d->documentElement->appendChild($r);
    return $r->replaceChild($d->createElement('n'), $d->createTextNode('y'));
});
cf('a DTD entity resolves too', function() {
    $d = new DOMDocument; $d->loadXML('<!DOCTYPE r [<!ENTITY e "x">]><r/>');
    $r = new DOMEntityReference('e');
    $d->documentElement->appendChild($r);
    return [$d->saveXML($d->documentElement), $r->childNodes->length];
});
cf('entref receiver is read-only', fn() => (new DOMEntityReference('amp'))->appendChild(new DOMText('t')));

/* all eight constructors, one roll call */
cf('roll call', function() {
    $mk = [new DOMElement('e'), new DOMText('t'), new DOMComment('c'), new DOMAttr('a', 'v'),
           new DOMCdataSection('cd'), new DOMDocumentFragment, new DOMProcessingInstruction('p'),
           new DOMEntityReference('amp')];
    $out = [];
    foreach ($mk as $n) { $out[] = get_class($n).':'.$n->nodeType.':'.var_export($n->ownerDocument === null, true); }
    return $out;
});
--EXPECT--
pi '': DOMException(5) Invalid Character Error
pi '1bad': DOMException(5) Invalid Character Error
pi 'a b': DOMException(5) Invalid Character Error
pi 'xml': 'xml'
pi 'XML': 'XML'
pi 'xml-stylesheet': 'xml-stylesheet'
pi 'p:a': 'p:a'
pi omitted data: array (
  0 => NULL,
  1 => '',
  2 => 't',
)
pi data literal: 'a?>b'
pi write before adoption: 'w'
frag shape: array (
  0 => '#document-fragment',
  1 => 11,
  2 => NULL,
  3 => 0,
)
frag append works: array (
  0 => 2,
  1 => 'txt',
)
frag appendChild refused: DOMException(7) No Modification Allowed Error
frag appendXML refused: DOMException(7) No Modification Allowed Error
built fragment splices whole: array (
  0 => '<r><k>v</k>txt</r>',
  1 => true,
  2 => true,
  3 => 0,
)
entref '': DOMException(5) Invalid Character Error
entref '1bad': DOMException(5) Invalid Character Error
entref 'a b': DOMException(5) Invalid Character Error
entref predefined resolves: array (
  0 => 'amp',
  1 => 1,
  2 => 17,
  3 => true,
)
entref unknown is empty: array (
  0 => NULL,
  1 => 0,
)
adopted entref serializes: '<r>&amp;&nosuch;</r>'
adopted entref reader still resolves: array (
  0 => 1,
  1 => 17,
)
adopted entref raw link is gone: false
a DTD entity resolves too: array (
  0 => '<r>&e;</r>',
  1 => 1,
)
entref receiver is read-only: DOMException(7) No Modification Allowed Error
roll call: array (
  0 => 'DOMElement:1:true',
  1 => 'DOMText:3:true',
  2 => 'DOMComment:8:true',
  3 => 'DOMAttr:2:true',
  4 => 'DOMCdataSection:4:true',
  5 => 'DOMDocumentFragment:11:true',
  6 => 'DOMProcessingInstruction:7:true',
  7 => 'DOMEntityReference:5:true',
)
