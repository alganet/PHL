--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
new DOMElement / new DOMAttr: the constructor's own name grammar, the entity-parsed element value vs the literal attribute value, and adoption through every door
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "W[$no]: $str\n"; return true; });
function ce($label, $fn) {
    try { $r = $fn(); echo "$label: ", is_object($r) ? get_class($r) : var_export($r, true), "\n"; }
    catch (Throwable $e) { echo "$label: ", get_class($e), '(', $e->getCode(), ') ', $e->getMessage(), "\n"; }
}

/* the element name grammar is the constructor's own: the whole name must be an
 * XML Name first, a prefix demands a namespace, and even the xml prefix with
 * its own URI is refused where createElementNS allows it */
foreach (['', '1bad', 'a b', '-x', 'x '] as $n) { ce("elem '$n'", fn() => new DOMElement($n)); }
foreach (['a', 'aé', 'x-', 'x.y', ':a'] as $n) { ce("elem '$n'", fn() => (new DOMElement($n))->tagName); }
foreach (['p:a', 'a:', 'xml:lang', 'xmlns:x', 'a:b:c'] as $n) { ce("elem '$n' no ns", fn() => new DOMElement($n)); }
ce("elem 'p:a' + ns", function() { $e = new DOMElement('p:a', null, 'urn:x'); return [$e->tagName, $e->prefix, $e->localName, $e->namespaceURI]; });
ce("elem 'a' + default ns", function() { $e = new DOMElement('a', null, 'urn:y'); return [$e->prefix, $e->namespaceURI]; });
ce("elem 'xmlns' + ns passes", fn() => (new DOMElement('xmlns', null, 'urn:x'))->namespaceURI);
foreach ([['p:a', ''], ['xml:a', 'http://www.w3.org/XML/1998/namespace'], ['xmlns:a', 'urn:x'],
          [':a', 'urn:x'], ['a:', 'urn:x'], ['p:1', 'urn:x'], ['a:b:c', 'urn:x']] as [$n, $u]) {
    ce("elem '$n' + '$u'", fn() => new DOMElement($n, null, $u));
}
ce("elem '1:a' + ns is the OTHER refusal", fn() => new DOMElement('1:a', null, 'urn:x'));

/* the element VALUE rides the entity parser; the attribute value is literal */
ce('elem value parses entities', function() { $e = new DOMElement('a', 'x&amp;y&#65;'); return [$e->nodeValue, $e->childNodes->length]; });
/* an UNTERMINATED reference ('v&x') is libxml's version's answer -- 2.9 warns
 * under this constructor's name and drops the whole value, 2.13 keeps 'vx' in
 * silence -- so the corpus leaves that cell unpinned (PLAN §7.4's class). */
ce('elem empty value has no child', function() { $e = new DOMElement('a', ''); return [$e->nodeValue, $e->firstChild]; });
ce('attr value literal', function() { $a = new DOMAttr('k', '&amp;'); return [$a->value, $a->childNodes->length]; });
ce('attr keeps a raw ampersand', fn() => (new DOMAttr('k', '&'))->value);
foreach (['', '1bad', 'a b'] as $n) { ce("attr '$n'", fn() => new DOMAttr($n, 'v')); }
ce("attr 'p:a' passes whole", function() { $a = new DOMAttr('p:a', 'v'); return [$a->name, $a->prefix, $a->localName, $a->namespaceURI]; });
ce("attr 'xmlns:x' too", fn() => (new DOMAttr('xmlns:x', 'v'))->name);

/* what a constructed element can already do */
ce('attribute surface works ownerless', function() {
    $e = new DOMElement('a');
    $e->setAttribute('k', 'v');
    $n = $e->getAttributeNode('k');
    return [$n->value, $n->ownerElement === $e, $e->attributes->length];
});
ce('two ownerless trees merge with identity', function() {
    $a = new DOMElement('r'); $b = new DOMElement('c', 't');
    $bt = $b->firstChild;
    $a->append($b);
    return [$a->firstChild === $b, $b->firstChild === $bt, $b->parentNode === $a];
});
ce('gebtn walks the ownerless subtree', function() {
    $e = new DOMElement('a');
    $e->append(new DOMElement('b'), new DOMElement('b'));
    return $e->getElementsByTagName('b')->length;
});
ce('insertAdjacent between ownerless elements', function() {
    $e = new DOMElement('a'); $x = new DOMElement('b');
    $r = $e->insertAdjacentElement('afterbegin', $x);
    return [$r === $x, $x->ownerDocument];
});

/* adoption: appendChild carries the whole tree, setAttributeNode the attribute,
 * and the reconcile declares the constructor's namespace at the landing site */
$d = new DOMDocument;
$d->loadXML('<r/>');
ce('adopt a built subtree', function() use ($d) {
    $a = new DOMElement('sub'); $a->append(new DOMElement('kid', 'txt'));
    $kid = $a->firstChild;
    $d->documentElement->appendChild($a);
    return [$a->ownerDocument === $d, $kid->ownerDocument === $d, $d->saveXML($d->documentElement)];
});
ce('adopt declares the ns', function() {
    $x = new DOMDocument; $x->loadXML('<r/>');
    $x->documentElement->appendChild(new DOMElement('p:a', 'v', 'urn:x'));
    $x->documentElement->appendChild(new DOMElement('dflt', null, 'urn:y'));
    return $x->saveXML($x->documentElement);
});
ce('setAttributeNode adopts', function() {
    $x = new DOMDocument; $x->loadXML('<r/>');
    $a = new DOMAttr('k', 'v');
    $old = $x->documentElement->setAttributeNode($a);
    return [$old, $a->ownerElement === $x->documentElement, $a->ownerDocument === $x, $x->saveXML($x->documentElement)];
});
ce('appendChild(attr) adopts as property', function() {
    $x = new DOMDocument; $x->loadXML('<r/>');
    $a = new DOMAttr('pk', 'pv');
    $x->documentElement->appendChild($a);
    return [$a->ownerDocument === $x, $x->saveXML($x->documentElement)];
});
ce('doc refuses the attr BEFORE adopting it', function() {
    $x = new DOMDocument; $a = new DOMAttr('k');
    try { $x->appendChild($a); } catch (DOMException $e) {}
    return [$a->ownerDocument];
});
ce('ownerless elem->appendChild(owned) is No Modification', function() use ($d) {
    $e = new DOMElement('o');
    return $e->appendChild($d->documentElement);
});
ce('ownerless elem->append(owned) is Wrong Document', function() use ($d) {
    $e = new DOMElement('o');
    $e->append($d->documentElement);
    return 'appended';
});
ce('lookup on the constructed ns', function() {
    $e = new DOMElement('p:a', null, 'urn:x');
    return [$e->lookupNamespaceURI('p'), $e->lookupPrefix('urn:x')];
});
ce('reflection', function() {
    $c = (new ReflectionClass('DOMElement'))->getConstructor();
    $out = [];
    foreach ($c->getParameters() as $p) {
        $out[] = ($p->getType() ? (string)$p->getType() : '?').' $'.$p->getName()
               .($p->isDefaultValueAvailable() ? ' = '.var_export($p->getDefaultValue(), true) : '');
    }
    return $out;
});
--EXPECT--
elem '': DOMException(5) Invalid Character Error
elem '1bad': DOMException(5) Invalid Character Error
elem 'a b': DOMException(5) Invalid Character Error
elem '-x': DOMException(5) Invalid Character Error
elem 'x ': DOMException(5) Invalid Character Error
elem 'a': 'a'
elem 'aé': 'aé'
elem 'x-': 'x-'
elem 'x.y': 'x.y'
elem ':a': ':a'
elem 'p:a' no ns: DOMException(14) Namespace Error
elem 'a:' no ns: DOMException(14) Namespace Error
elem 'xml:lang' no ns: DOMException(14) Namespace Error
elem 'xmlns:x' no ns: DOMException(14) Namespace Error
elem 'a:b:c' no ns: DOMException(14) Namespace Error
elem 'p:a' + ns: array (
  0 => 'p:a',
  1 => 'p',
  2 => 'a',
  3 => 'urn:x',
)
elem 'a' + default ns: array (
  0 => '',
  1 => 'urn:y',
)
elem 'xmlns' + ns passes: 'urn:x'
elem 'p:a' + '': DOMException(14) Namespace Error
elem 'xml:a' + 'http://www.w3.org/XML/1998/namespace': DOMException(14) Namespace Error
elem 'xmlns:a' + 'urn:x': DOMException(14) Namespace Error
elem ':a' + 'urn:x': DOMException(14) Namespace Error
elem 'a:' + 'urn:x': DOMException(14) Namespace Error
elem 'p:1' + 'urn:x': DOMException(14) Namespace Error
elem 'a:b:c' + 'urn:x': DOMException(14) Namespace Error
elem '1:a' + ns is the OTHER refusal: DOMException(5) Invalid Character Error
elem value parses entities: array (
  0 => 'x&yA',
  1 => 1,
)
elem empty value has no child: array (
  0 => '',
  1 => NULL,
)
attr value literal: array (
  0 => '&amp;',
  1 => 1,
)
attr keeps a raw ampersand: '&'
attr '': DOMException(5) Invalid Character Error
attr '1bad': DOMException(5) Invalid Character Error
attr 'a b': DOMException(5) Invalid Character Error
attr 'p:a' passes whole: array (
  0 => 'p:a',
  1 => '',
  2 => 'p:a',
  3 => NULL,
)
attr 'xmlns:x' too: 'xmlns:x'
attribute surface works ownerless: array (
  0 => 'v',
  1 => true,
  2 => 1,
)
two ownerless trees merge with identity: array (
  0 => true,
  1 => true,
  2 => true,
)
gebtn walks the ownerless subtree: 2
insertAdjacent between ownerless elements: array (
  0 => true,
  1 => NULL,
)
adopt a built subtree: array (
  0 => true,
  1 => true,
  2 => '<r><sub><kid>txt</kid></sub></r>',
)
adopt declares the ns: '<r><p:a xmlns:p="urn:x">v</p:a><dflt xmlns="urn:y"/></r>'
setAttributeNode adopts: array (
  0 => NULL,
  1 => true,
  2 => true,
  3 => '<r k="v"/>',
)
appendChild(attr) adopts as property: array (
  0 => true,
  1 => '<r pk="pv"/>',
)
doc refuses the attr BEFORE adopting it: array (
  0 => NULL,
)
ownerless elem->appendChild(owned) is No Modification: DOMException(7) No Modification Allowed Error
ownerless elem->append(owned) is Wrong Document: DOMException(4) Wrong Document Error
lookup on the constructed ns: array (
  0 => 'urn:x',
  1 => 'p',
)
reflection: array (
  0 => 'string $qualifiedName',
  1 => '?string $value = NULL',
  2 => 'string $namespace = \'\'',
)
