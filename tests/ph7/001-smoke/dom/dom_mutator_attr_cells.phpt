--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
appendChild/insertBefore attach an ATTRIBUTE as a property; the level-2 doors' odd cells answer php's taxonomy (silent false, No Modification, Hierarchy, the sibling Error)
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "W[$no]: $str\n"; return true; });
function cell($label, $fn) {
    try { $r = $fn(); echo "$label: ", is_object($r) ? get_class($r) : var_export($r, true), "\n"; }
    catch (Throwable $e) { echo "$label: ", get_class($e), '(', $e->getCode(), ') ', $e->getMessage(), "\n"; }
}

/* appendChild(attr) is a spelling of setAttributeNode: attach, replace, move, reorder */
$d = new DOMDocument;
$d->loadXML('<r k="old" j="1"><c b="2">t</c></r>');
$r = $d->documentElement;
$a = $d->createAttribute('k'); $a->value = 'new';
cell('replace', fn() => $r->appendChild($a) === $a ? $d->saveXML($r) : 'lost');
cell('identity', fn() => $r->getAttributeNode('k') === $a);
cell('reorder own attr to tail', function() use ($d, $r) { $r->appendChild($r->getAttributeNode('j')); return $d->saveXML($r); });
cell('move between elements', function() use ($d, $r) { $r->firstChild->appendChild($r->getAttributeNode('j')); return $d->saveXML($r); });
$d2 = new DOMDocument;
$d2->loadXML('<r xmlns:p="urn:x" p:k="old"/>');
cell('ns lookup replaces', function() use ($d2) {
    $na = $d2->createAttributeNS('urn:x', 'p:k'); $na->value = 'new';
    $d2->documentElement->appendChild($na);
    return $d2->saveXML($d2->documentElement);
});
cell('plain name leaves the namespaced one', function() use ($d2) {
    $pa = $d2->createAttribute('k'); $pa->value = 'plain';
    $d2->documentElement->appendChild($pa);
    return $d2->saveXML($d2->documentElement);
});

/* insertBefore: a null reference attaches; an ATTRIBUTE reference means the property
 * list's order; any other reference is the sibling Error -- after the displacement
 * and the unlink, php's own order */
$d3 = new DOMDocument;
$d3->loadXML('<r z="9"><c/></r>');
$r3 = $d3->documentElement;
cell('insertBefore(attr, null)', function() use ($d3, $r3) { $r3->insertBefore($d3->createAttribute('nA'), null); return $d3->saveXML($r3); });
cell('insertBefore(attr, attr-ref)', function() use ($d3, $r3) { $r3->insertBefore($d3->createAttribute('first'), $r3->getAttributeNode('z')); return $d3->saveXML($r3); });
cell('insertBefore(attr, elem-ref) is the Error', function() use ($d3, $r3) {
    try { $r3->insertBefore($d3->createAttribute('x'), $r3->firstChild); }
    catch (Error $e) { return $e->getMessage().' / attached: '.var_export($r3->hasAttribute('x'), true); }
    return 'attached';
});
cell('...and it costs the displaced old + the unlinked argument', function() use ($d3, $r3) {
    $victim = $d3->createAttribute('z'); $victim->value = 'vz';
    try { $r3->insertBefore($victim, $r3->firstChild); } catch (Error $e) {}
    return $d3->saveXML($r3);
});
cell('insertBefore(attr, foreign-ref)', function() use ($d3, $r3) { return $r3->insertBefore($d3->createAttribute('x'), $d3->createElement('nope')); });
cell('a same-name argument takes its own reference down', function() use ($d3) {
    $x = new DOMDocument; $x->loadXML('<r b="old" z="9"/>');
    $n = $x->createAttribute('b'); $n->value = 'new';
    try { $x->documentElement->insertBefore($n, $x->documentElement->getAttributeNode('b')); }
    catch (Error $e) {}
    return $x->saveXML($x->documentElement).' / '.var_export($n->ownerElement === null, true);
});
cell('an attribute as its own reference', function() {
    $x = new DOMDocument; $x->loadXML('<r b="old" z="9"/>');
    $b = $x->documentElement->getAttributeNode('b');
    try { $x->documentElement->insertBefore($b, $b); }
    catch (Error $e) { return $e->getMessage().' / '.$x->saveXML($x->documentElement); }
    return 'linked';
});

/* removeChild: membership is `has children at all + parent pointer`, which really
 * does remove an attribute -- but only off an element with at least one child */
cell('removeChild(attr) with children', function() { $x = new DOMDocument; $x->loadXML('<r k="v"><c/></r>'); $x->documentElement->removeChild($x->documentElement->getAttributeNode('k')); return $x->saveXML($x->documentElement); });
cell('removeChild(attr) childless', function() { $x = new DOMDocument; $x->loadXML('<r k="v"/>'); return $x->documentElement->removeChild($x->documentElement->getAttributeNode('k')); });
cell('removeChild(entref child)', function() { $x = new DOMDocument; $x->loadXML('<!DOCTYPE r [<!ENTITY e "x">]><r>&e;</r>'); $er = $x->documentElement->firstChild; return $er->removeChild($er->firstChild); });

/* replaceChild: wrong document first, two silent-false cells, read-only, the
 * attributes-together-or-not-at-all XOR, membership last */
$mk = function() { $x = new DOMDocument; $x->loadXML('<!DOCTYPE r [<!ENTITY e "x">]><r a="1">&e;<c b="2">t</c></r>'); return $x; };
cell('text recv + foreign arg', function() use ($mk) { $x = $mk(); $y = new DOMDocument; $y->loadXML('<z/>'); return $x->documentElement->lastChild->firstChild->replaceChild($y->documentElement, $x->createElement('v')); });
cell('text recv + same-doc arg', function() use ($mk) { $x = $mk(); return $x->documentElement->lastChild->firstChild->replaceChild($x->createElement('n'), $x->createElement('v')); });
cell('childless recv', function() use ($mk) { $x = $mk(); $e = $x->createElement('empty'); return $e->replaceChild($x->createElement('n'), $x->createElement('v')); });
cell('entref recv', function() use ($mk) { $x = $mk(); $er = $x->documentElement->firstChild; return $er->replaceChild($x->createElement('n'), $er->firstChild); });
cell('attr recv + comment arg', function() use ($mk) { $x = $mk(); $b = $x->documentElement->lastChild->getAttributeNode('b'); return $b->replaceChild($x->createComment('c'), $b->firstChild); });
cell('attr recv + text arg', function() use ($mk) { $x = $mk(); $b = $x->documentElement->lastChild->getAttributeNode('b'); $b->replaceChild($x->createTextNode('W'), $b->firstChild); return $b->value; });
cell('attr arg + elem victim', function() use ($mk) { $x = $mk(); return $x->documentElement->replaceChild($x->createAttribute('n'), $x->documentElement->lastChild); });
cell('elem arg + attr victim', function() use ($mk) { $x = $mk(); return $x->documentElement->replaceChild($x->createElement('n'), $x->documentElement->getAttributeNode('a')); });
cell('attr arg + foreign attr victim', function() use ($mk) { $x = $mk(); $y = new DOMDocument; $y->loadXML('<q z="9"/>'); return $x->documentElement->replaceChild($x->createAttribute('n'), $x->documentElement->getAttributeNode('a')) === null ? 'null' : 'swap'; });
cell('attr swaps attr in place', function() use ($mk) {
    $x = $mk(); $el = $x->documentElement->lastChild;
    $n = $x->createAttribute('b2'); $n->value = 'nv';
    $old = $el->replaceChild($n, $el->getAttributeNode('b'));
    return get_class($old).' / '.$x->saveXML($el);
});
cell('doc arg', function() use ($mk) { $x = $mk(); return $x->documentElement->replaceChild($x, $x->documentElement->lastChild); });

/* the invalid-children receivers answer FALSE with nothing said, before any
 * other screen -- even a foreign argument */
$d4 = new DOMDocument;
$d4->loadXML('<r>t<!--c--><?pi d?><![CDATA[cd]]></r>');
foreach ([0 => 'text', 1 => 'comment', 2 => 'pi', 3 => 'cdata'] as $i => $kind) {
    cell("appendChild on $kind", fn() => $d4->documentElement->childNodes->item($i)->appendChild($d4->createElement('x')));
}
cell('foreign arg still false', function() use ($d4) { $y = new DOMDocument; $y->loadXML('<z/>'); return $d4->documentElement->firstChild->appendChild($y->documentElement); });

/* entity-reference receivers are the read-only refusal; attribute receivers take
 * text and entity references only; a document child and a self-append are Hierarchy */
$d5 = new DOMDocument;
$d5->loadXML('<!DOCTYPE r [<!ENTITY e "x">]><r k="v">&e;</r>');
cell('entref recv appendChild', fn() => $d5->documentElement->firstChild->appendChild($d5->createTextNode('t')));
cell('attr recv + text', function() use ($d5) { $a = $d5->documentElement->getAttributeNode('k'); $a->appendChild($d5->createTextNode('X')); return $a->value; });
cell('attr recv + entref', function() use ($d5) { $a = $d5->documentElement->getAttributeNode('k'); $a->appendChild($d5->createEntityReference('e')); return $d5->saveXML($d5->documentElement); });
cell('attr recv + elem', fn() => $d5->documentElement->getAttributeNode('k')->appendChild($d5->createElement('x')));
cell('attr recv + empty frag', fn() => $d5->documentElement->getAttributeNode('k')->appendChild($d5->createDocumentFragment()));
cell('attr recv + frag with text', function() use ($d5) { $f = $d5->createDocumentFragment(); $f->appendChild($d5->createTextNode('T')); return $d5->documentElement->getAttributeNode('k')->appendChild($f); });
cell('doc recv + attr', fn() => (new DOMDocument)->appendChild($d5->documentElement->getAttributeNode('k')));
cell('orphan elem + own doc', function() use ($d5) { return $d5->createElement('o')->appendChild($d5); });
cell('frag appendChild(itself)', function() use ($d5) { $f = $d5->createDocumentFragment(); return $f->appendChild($f); });
cell('doc recv + text is allowed', function() { $x = new DOMDocument; $x->appendChild($x->createTextNode('t')); return $x->saveXML(); });
cell('two roots are allowed', function() { $x = new DOMDocument; $x->loadXML('<r/>'); $x->appendChild($x->createElement('x')); return $x->saveXML(); });
--EXPECT--
replace: '<r j="1" k="new"><c b="2">t</c></r>'
identity: true
reorder own attr to tail: '<r k="new" j="1"><c b="2">t</c></r>'
move between elements: '<r k="new"><c b="2" j="1">t</c></r>'
ns lookup replaces: '<r xmlns:p="urn:x" p:k="new"/>'
plain name leaves the namespaced one: '<r xmlns:p="urn:x" p:k="new" k="plain"/>'
insertBefore(attr, null): '<r z="9" nA=""><c/></r>'
insertBefore(attr, attr-ref): '<r first="" z="9" nA=""><c/></r>'
insertBefore(attr, elem-ref) is the Error: 'Cannot add newnode as the previous sibling of refnode / attached: false'
...and it costs the displaced old + the unlinked argument: '<r first="" nA=""><c/></r>'
insertBefore(attr, foreign-ref): DOMException(8) Not Found Error
a same-name argument takes its own reference down: '<r z="9"/> / true'
an attribute as its own reference: 'Cannot add newnode as the previous sibling of refnode / <r z="9"/>'
removeChild(attr) with children: '<r><c/></r>'
removeChild(attr) childless: DOMException(8) Not Found Error
removeChild(entref child): DOMException(8) Not Found Error
text recv + foreign arg: DOMException(4) Wrong Document Error
text recv + same-doc arg: false
childless recv: false
entref recv: DOMException(7) No Modification Allowed Error
attr recv + comment arg: DOMException(3) Hierarchy Request Error
attr recv + text arg: 'W'
attr arg + elem victim: DOMException(3) Hierarchy Request Error
elem arg + attr victim: DOMException(3) Hierarchy Request Error
attr arg + foreign attr victim: 'swap'
attr swaps attr in place: 'DOMAttr / <c b2="nv">t</c>'
doc arg: DOMException(3) Hierarchy Request Error
appendChild on text: false
appendChild on comment: false
appendChild on pi: false
appendChild on cdata: false
foreign arg still false: false
entref recv appendChild: DOMException(7) No Modification Allowed Error
attr recv + text: 'vX'
attr recv + entref: '<r k="vX&e;">&e;</r>'
attr recv + elem: DOMException(3) Hierarchy Request Error
W[2]: DOMNode::appendChild(): Document Fragment is empty
attr recv + empty frag: false
attr recv + frag with text: DOMException(3) Hierarchy Request Error
doc recv + attr: DOMException(4) Wrong Document Error
orphan elem + own doc: DOMException(3) Hierarchy Request Error
frag appendChild(itself): DOMException(3) Hierarchy Request Error
doc recv + text is allowed: '<?xml version="1.0"?>
t
'
two roots are allowed: '<?xml version="1.0"?>
<r/>
<x/>
'
