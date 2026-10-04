--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\Element attributes: an absent one is null, the two writes are void
--FILE--
<?php
function mea_show($v) {
	if ($v === null) { return 'null'; }
	if ($v === false) { return 'false'; }
	if ($v === true) { return 'true'; }
	if (is_string($v)) { return "'" . $v . "'"; }
	if (is_array($v)) { return '[' . implode(',', array_map('mea_show', $v)) . ']'; }
	if ($v instanceof Dom\Attr) { return get_class($v) . ' ' . $v->name . '=' . $v->value; }
	return get_class($v);
}
function mea_try($zLabel, $cb) {
	try {
		echo $zLabel, ' => ', mea_show($cb()), "\n";
	} catch (Throwable $e) {
		echo $zLabel, ' !! ', get_class($e), '(', $e->getCode(), '): ', $e->getMessage(), "\n";
	}
}
$mea_doc = Dom\XMLDocument::createFromString('<r xmlns:x="urn:x"><e a="1" x:b="2" empty=""/></r>');
$mea_el = $mea_doc->documentElement->firstChild;

/* An absent attribute reads null here and "" in the 2004 tree. */
mea_try('getAttribute a',        fn() => $mea_el->getAttribute('a'));
mea_try('getAttribute empty',    fn() => $mea_el->getAttribute('empty'));
mea_try('getAttribute absent',   fn() => $mea_el->getAttribute('nope'));
mea_try('getAttribute x:b',      fn() => $mea_el->getAttribute('x:b'));
mea_try('getAttributeNS bound',  fn() => $mea_el->getAttributeNS('urn:x', 'b'));
mea_try('getAttributeNS none',   fn() => $mea_el->getAttributeNS(null, 'a'));
mea_try('getAttributeNS absent', fn() => $mea_el->getAttributeNS('urn:zz', 'b'));
mea_try('getAttributeNames',     fn() => $mea_el->getAttributeNames());
mea_try('hasAttribute a',        fn() => $mea_el->hasAttribute('a'));
mea_try('hasAttribute absent',   fn() => $mea_el->hasAttribute('nope'));
mea_try('hasAttributeNS',        fn() => $mea_el->hasAttributeNS('urn:x', 'b'));
mea_try('hasAttributeNS absent', fn() => $mea_el->hasAttributeNS('urn:zz', 'b'));

/* A namespace DECLARATION is an attribute to this question. */
mea_try('hasAttributes',         fn() => $mea_el->hasAttributes());
mea_try('hasAttributes decl',    fn() => $mea_doc->documentElement->hasAttributes());
mea_try('names decl',            fn() => $mea_doc->documentElement->getAttributeNames());

/* An absent NODE is null, not the 2004 false. */
mea_try('getAttributeNode a',      fn() => $mea_el->getAttributeNode('a'));
mea_try('getAttributeNode absent', fn() => $mea_el->getAttributeNode('nope'));
mea_try('getAttributeNodeNS',      fn() => $mea_el->getAttributeNodeNS('urn:x', 'b'));
mea_try('getAttributeNodeNS absent', fn() => $mea_el->getAttributeNodeNS('urn:zz', 'b'));

/* The two writes are void here and handed back a node in the 2004 tree. */
mea_try('setAttribute',          fn() => $mea_el->setAttribute('c', '3'));
mea_try('read it back',          fn() => $mea_el->getAttribute('c'));
mea_try('setAttribute bad name', fn() => $mea_el->setAttribute('1bad', 'x'));
mea_try('setAttributeNS',        fn() => $mea_el->setAttributeNS('urn:y', 'y:d', '4'));
mea_try('removeAttribute',       fn() => $mea_el->removeAttribute('c'));
mea_try('removeAttribute absent', fn() => $mea_el->removeAttribute('nope'));
mea_try('removeAttributeNS',     fn() => $mea_el->removeAttributeNS('urn:y', 'd'));
mea_try('toggleAttribute on',    fn() => $mea_el->toggleAttribute('t'));
mea_try('toggleAttribute off',   fn() => $mea_el->toggleAttribute('t'));
mea_try('toggleAttribute force', fn() => $mea_el->toggleAttribute('t', false));

/* The node writes keep their 2004 shapes: null unless one was replaced. */
$mea_attr = $mea_el->getAttributeNode('a');
mea_try('removeAttributeNode',   fn() => $mea_el->removeAttributeNode($mea_attr));
mea_try('removeAttributeNode again', fn() => $mea_el->removeAttributeNode($mea_attr));
mea_try('setAttributeNode',      fn() => $mea_el->setAttributeNode($mea_attr));
mea_try('setIdAttribute',        fn() => $mea_el->setIdAttribute('a', true));
mea_try('setIdAttributeNS null', fn() => $mea_el->setIdAttributeNS(null, 'a', true));
mea_try('setIdAttributeNode',    fn() => $mea_el->setIdAttributeNode($mea_el->getAttributeNode('a'), true));
mea_try('setIdAttribute absent', fn() => $mea_el->setIdAttribute('nope', true));

/* Type declarations, as the class states them. */
$mea_r = new ReflectionClass('Dom\Element');
foreach (['getAttribute', 'getAttributeNode', 'setAttribute', 'removeAttribute',
          'removeAttributeNode', 'setIdAttributeNS'] as $mea_n) {
	$mea_m = $mea_r->getMethod($mea_n);
	echo $mea_n, ': ', (string) $mea_m->getReturnType(), "\n";
}
--EXPECT--
getAttribute a => '1'
getAttribute empty => ''
getAttribute absent => null
getAttribute x:b => '2'
getAttributeNS bound => '2'
getAttributeNS none => '1'
getAttributeNS absent => null
getAttributeNames => ['a','x:b','empty']
hasAttribute a => true
hasAttribute absent => false
hasAttributeNS => true
hasAttributeNS absent => false
hasAttributes => true
hasAttributes decl => true
names decl => ['xmlns:x']
getAttributeNode a => Dom\Attr a=1
getAttributeNode absent => null
getAttributeNodeNS => Dom\Attr x:b=2
getAttributeNodeNS absent => null
setAttribute => null
read it back => '3'
setAttribute bad name => setAttribute bad name !! DOMException(5): Invalid Character Error
setAttributeNS => null
removeAttribute => null
removeAttribute absent => null
removeAttributeNS => null
toggleAttribute on => true
toggleAttribute off => false
toggleAttribute force => false
removeAttributeNode => Dom\Attr a=1
removeAttributeNode again => removeAttributeNode again !! DOMException(8): Not Found Error
setAttributeNode => null
setIdAttribute => null
setIdAttributeNS null => setIdAttributeNS null !! DOMException(8): Not Found Error
setIdAttributeNode => null
setIdAttribute absent => setIdAttribute absent !! DOMException(8): Not Found Error
getAttribute: ?string
getAttributeNode: ?Dom\Attr
setAttribute: void
removeAttribute: void
removeAttributeNode: Dom\Attr
setIdAttributeNS: void
