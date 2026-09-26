--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
new DOMText/DOMComment/DOMCdataSection build a real ownerless node: null owner until an insertion ADOPTS it, No Modification from the child-list doors, working edits and identity throughout
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "W[$no]: $str\n"; return true; });
function cc($label, $fn) {
    try { $r = $fn(); echo "$label: ", is_object($r) ? get_class($r) : var_export($r, true), "\n"; }
    catch (Throwable $e) { echo "$label: ", get_class($e), '(', $e->getCode(), ') ', $e->getMessage(), "\n"; }
}

/* the omitted argument reads NULL where the explicit empty string reads "" */
$t = new DOMText;
cc('text0', fn() => [$t->nodeValue, $t->nodeName, $t->nodeType, $t->ownerDocument, $t->parentNode, $t->isConnected]);
cc('text empty', fn() => (new DOMText(''))->nodeValue);
cc('text data', function() { $x = new DOMText('abé'); return [$x->data, $x->length, $x->wholeText]; });
cc('comment', fn() => [(new DOMComment)->nodeValue, (new DOMComment(''))->nodeValue, (new DOMComment('a--b'))->data]);
cc('cdata', function() { $c = new DOMCdataSection('a]]>b'); return [$c->data, $c->nodeName, $c->nodeType]; });
cc('cdata requires data', fn() => new DOMCdataSection);

/* an ownerless node EDITS -- and splits -- but its child list is read-only.
 * The edit family reads the omitted-argument NULL content as "": the range
 * still screens, a writer normalizes it, and `data` coerces where `nodeValue`
 * does not */
cc('edits work', function() { $x = new DOMText('t'); $x->data = 'w'; $x->appendData('x'); return $x->data; });
cc('null-content reads', function() { $x = new DOMText; return [$x->substringData(0, 1), $x->data, $x->nodeValue]; });
cc('null-content range still screens', fn() => (new DOMText)->substringData(5, 1));
cc('null-content writer normalizes', function() { $x = new DOMComment; $x->insertData(0, 'i'); return [$x->data, $x->nodeValue]; });
cc('null-content delete leaves ""', function() { $x = new DOMText; $x->deleteData(0, 1); return $x->nodeValue; });
cc('null-content split', function() { $x = new DOMText; $s = $x->splitText(0); return [get_class($s), $s->data]; });
cc('splitText works', function() { $x = new DOMText('abcd'); $s = $x->splitText(2); return [$s->data, $x->data, $s->previousSibling]; });
cc('appendChild refused', fn() => (new DOMText('t'))->appendChild(new DOMText('u')));
cc('replaceWith refused', function() { (new DOMText('t'))->replaceWith('z'); return 'silent'; });
cc('remove refused', function() { (new DOMText('t'))->remove(); return 'silent'; });
cc('before is the parentless silence', function() { (new DOMText('t'))->before('b'); return 'silent'; });

/* the level-2 doors ADOPT; the modern family refuses with Wrong Document */
$d = new DOMDocument;
$d->loadXML('<r><c/></r>');
$n = new DOMText('T');
cc('appendChild adopts', fn() => [$d->documentElement->appendChild($n) === $n, $n->ownerDocument === $d, $d->saveXML($d->documentElement)]);
cc('identity after adoption', fn() => [$d->documentElement->lastChild === $n, $n->parentNode === $d->documentElement]);
cc('insertBefore + replaceChild adopt', function() use ($d) {
    $d->documentElement->insertBefore(new DOMComment('i'), $d->documentElement->firstChild);
    $d->documentElement->replaceChild(new DOMCdataSection('cd'), $d->documentElement->firstChild->nextSibling);
    return $d->saveXML($d->documentElement);
});
cc('append() refuses ownerless', fn() => $d->documentElement->append(new DOMText('x')));
cc('lax append warns and drops', function() {
    $x = new DOMDocument; $x->loadXML('<q/>'); $x->strictErrorChecking = false;
    $x->documentElement->append(new DOMText('t'));
    return $x->saveXML($x->documentElement);
});
cc('adoptNode keeps the object', function() { $x = new DOMDocument; $c = new DOMComment('a'); return [$x->adoptNode($c) === $c, $c->ownerDocument === $x]; });
cc('importNode copies instead', function() { $x = new DOMDocument; $c = new DOMText('y'); $i = $x->importNode($c); return [$i === $c, $i->ownerDocument === $x, $c->ownerDocument]; });

/* what an ownerless node cannot do, and what it still answers */
cc('C14N needs a document', fn() => (new DOMText('t'))->C14N());
cc('saveXML(ownerless) is Wrong Document', function() { $x = new DOMDocument; return $x->saveXML(new DOMText('t')); });
cc('xpath context refused', function() {
    $x = new DOMDocument; $x->loadXML('<q/>');
    return (new DOMXPath($x))->query('.', new DOMText('t'));
});
cc('getNodePath / getLineNo / baseURI', function() { $x = new DOMText('z'); return [$x->getNodePath(), $x->getLineNo(), $x->baseURI]; });
cc('clone stays ownerless', function() { $x = new DOMCdataSection('cl'); $c = clone $x; return [$c !== $x, $c->data, $c->ownerDocument]; });
cc('cloneNode too', function() { $x = new DOMText('cn'); $c = $x->cloneNode(); return [get_class($c), $c->data, $c === $x]; });
cc('isEqualNode across the divide', function() {
    $x = new DOMDocument; $x->loadXML('<a>v</a>');
    return (new DOMText('v'))->isEqualNode($x->documentElement->firstChild);
});

/* the constructor is an ordinary method: re-running it re-points the node, and
 * Reflection reports php's signature */
cc('ctor re-run', function() { $x = new DOMText('a'); $x->__construct('b'); return $x->data; });
cc('reflection', function() {
    $p = (new ReflectionClass('DOMComment'))->getConstructor()->getParameters()[0];
    return [(string)$p->getType(), $p->getName(), var_export($p->getDefaultValue(), true)];
});
--EXPECT--
text0: array (
  0 => NULL,
  1 => '#text',
  2 => 3,
  3 => NULL,
  4 => NULL,
  5 => false,
)
text empty: ''
text data: array (
  0 => 'abé',
  1 => 3,
  2 => 'abé',
)
comment: array (
  0 => NULL,
  1 => '',
  2 => 'a--b',
)
cdata: array (
  0 => 'a]]>b',
  1 => '#cdata-section',
  2 => 4,
)
cdata requires data: ArgumentCountError(0) DOMCdataSection::__construct() expects exactly 1 argument, 0 given
edits work: 'wx'
null-content reads: array (
  0 => '',
  1 => '',
  2 => NULL,
)
null-content range still screens: DOMException(1) Index Size Error
null-content writer normalizes: array (
  0 => 'i',
  1 => 'i',
)
null-content delete leaves "": ''
null-content split: array (
  0 => 'DOMText',
  1 => '',
)
splitText works: array (
  0 => 'cd',
  1 => 'ab',
  2 => NULL,
)
appendChild refused: false
replaceWith refused: 'silent'
remove refused: DOMException(7) No Modification Allowed Error
before is the parentless silence: 'silent'
appendChild adopts: array (
  0 => true,
  1 => true,
  2 => '<r><c/>T</r>',
)
identity after adoption: array (
  0 => true,
  1 => true,
)
insertBefore + replaceChild adopt: '<r><!--i--><![CDATA[cd]]>T</r>'
append() refuses ownerless: DOMException(4) Wrong Document Error
W[2]: DOMElement::append(): Wrong Document Error
lax append warns and drops: '<q/>'
adoptNode keeps the object: array (
  0 => true,
  1 => true,
)
importNode copies instead: array (
  0 => false,
  1 => true,
  2 => NULL,
)
C14N needs a document: Error(0) Node must be associated with a document
saveXML(ownerless) is Wrong Document: DOMException(4) Wrong Document Error
xpath context refused: Error(0) Node from wrong document
getNodePath / getLineNo / baseURI: array (
  0 => '/text()',
  1 => 0,
  2 => NULL,
)
clone stays ownerless: array (
  0 => true,
  1 => 'cl',
  2 => NULL,
)
cloneNode too: array (
  0 => 'DOMText',
  1 => 'cn',
  2 => false,
)
isEqualNode across the divide: true
ctor re-run: 'b'
reflection: array (
  0 => 'string',
  1 => 'data',
  2 => '\'\'',
)
