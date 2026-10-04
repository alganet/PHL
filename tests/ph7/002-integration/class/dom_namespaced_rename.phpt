--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A namespaced element or attribute can be renamed into another namespace
--FILE--
<?php
/* php 8.4's `rename()` is the one door that changes a node's name AND its
 * namespace at once; the 2004 tree has none, so a program that wants one there
 * rebuilds the node and moves every child by hand.
 *
 * The name is judged exactly as the namespaced attribute FACTORY judges one,
 * on BOTH receivers: a QName always, so `:z`, `z:` and `a:b:c` are the Invalid
 * Character Error even on an element, where the namespaced element factory
 * would have written `:z` literally. Every rule ABOUT a namespace -- a prefix
 * with none, the `xml` prefix off its own URI, the xmlns pairing -- is still
 * the Namespace Error, because those are rules and not spellings.
 *
 * Where the binding GOES is what the two receivers do not share. An element
 * declares it on itself, so what its descendants resolve changes with it --
 * and it declares under the prefix ASKED FOR even when that shadows an outer
 * binding of the same prefix. An attribute declares nothing at all: it carries
 * a free-standing binding, exactly as the namespaced attribute factory hands
 * one out, so no descendant sees anything and the prefix a serializer writes
 * is that serializer's own decision.
 *
 * An attribute is the only receiver that can collide, and the refusal is prose
 * under code 13 rather than one of the level-2 names. It is asked of the owner
 * element, so renaming an attribute to the name it already has is not a
 * collision with itself. */
function tree() {
	return Dom\XMLDocument::createFromString(
		'<r xmlns:p="urn:p"><a p:x="1" y="2"><k/></a></r>');
}
function shape($node) {
	return $node->nodeName . ' local=' . $node->localName
		. ' prefix=' . var_export($node->prefix, true)
		. ' uri=' . var_export($node->namespaceURI, true);
}
/* Both receivers, one grammar. */
foreach ([null, '', 'urn:n', 'urn:p', 'http://www.w3.org/XML/1998/namespace',
          'http://www.w3.org/2000/xmlns/'] as $ns) {
	foreach (['z', 'p:z', 'q:z', '', '1bad', ':z', 'z:', 'a:b:c', 'xml:z', 'xmlns',
	          'xmlns:z'] as $qn) {
		foreach (['element', 'attribute'] as $what) {
			$d = tree();
			$el = $d->documentElement->firstElementChild;
			$node = $what === 'element' ? $el : $el->getAttributeNodeNS('urn:p', 'x');
			printf('%-9s %-38s %-8s ', $what, var_export($ns, true), var_export($qn, true));
			try {
				$node->rename($ns, $qn);
				echo shape($node), "\n";
			} catch (DOMException $e) {
				echo get_class($e), '(', $e->getCode(), ') ', $e->getMessage(), "\n";
			}
		}
	}
}
/* What a DESCENDANT resolves afterwards -- the whole difference between the two
 * receivers, and invisible to any test that only reads the renamed node back. */
foreach ([['element', 'urn:n', 'q:z'], ['element', 'urn:n', 'z'],
          ['element', 'urn:p', 'p:z'], ['element', 'urn:n', 'p:z'],
          ['attribute', 'urn:n', 'q:z'], ['attribute', 'urn:n', 'z'],
          ['attribute', 'urn:p', 'q:z'], ['attribute', 'urn:n', 'p:z']] as $row) {
	[$what, $ns, $qn] = $row;
	$d = tree();
	$el = $d->documentElement->firstElementChild;
	$k = $el->firstElementChild;
	$node = $what === 'element' ? $el : $el->getAttributeNodeNS('urn:p', 'x');
	$node->rename($ns, $qn);
	printf("child of %-9s %-6s %-4s q=%-8s p=%-8s default=%-8s prefix(%s)=%s\n",
		$what, $ns, $qn,
		var_export($k->lookupNamespaceURI('q'), true),
		var_export($k->lookupNamespaceURI('p'), true),
		var_export($k->lookupNamespaceURI(null), true),
		$ns, var_export($k->lookupPrefix($ns), true));
}
/* The collision, and the two things that are not one. */
$d = tree();
$el = $d->documentElement->firstElementChild;
$at = $el->getAttributeNodeNS('urn:p', 'x');
try { $at->rename(null, 'y'); }
catch (DOMException $e) { echo 'collide: ', $e->getCode(), ' ', $e->getMessage(), "\n"; }
$at->rename('urn:p', 'p:x');
echo 'itself: ', shape($at), "\n";
$loose = $d->createAttribute('q');
$loose->rename(null, 'y');
echo 'detached: ', shape($loose), "\n";
/* Renaming an element out of the way frees the name for the next one. */
$d = tree();
$el = $d->documentElement->firstElementChild;
$el->rename(null, 'b');
echo 'element: ', $d->saveXml($d->documentElement), "\n";
/* `isId` is the other method php's namespaced attribute declares, and the ID
 * flag it reads is libxml's -- the tree asking makes no difference to it. */
$d = Dom\XMLDocument::createFromString(
	"<!DOCTYPE r [\n<!ELEMENT r ANY>\n<!ELEMENT a ANY>\n<!ATTLIST a id ID #IMPLIED>\n]>\n"
	. '<r><a id="i1" y="2"/></r>');
$a = $d->documentElement->firstElementChild;
var_dump($a->getAttributeNode('id')->isId(), $a->getAttributeNode('y')->isId());
$a->setIdAttribute('y', true);
var_dump($a->getAttributeNode('y')->isId());
$a->setIdAttribute('y', false);
var_dump($a->getAttributeNode('y')->isId());
var_dump($d->createAttribute('q')->isId());
/* And php declares exactly those two on it, in this order. */
$r = new ReflectionClass('Dom\\Attr');
$own = [];
foreach ($r->getMethods() as $m) {
	if ($m->getDeclaringClass()->getName() === 'Dom\\Attr') { $own[] = $m->getName(); }
}
echo 'Dom\\Attr declares: ', implode(',', $own), "\n";
?>
--EXPECT--
element   NULL                                   'z'      z local=z prefix=NULL uri=NULL
attribute NULL                                   'z'      z local=z prefix=NULL uri=NULL
element   NULL                                   'p:z'    DOMException(14) Namespace Error
attribute NULL                                   'p:z'    DOMException(14) Namespace Error
element   NULL                                   'q:z'    DOMException(14) Namespace Error
attribute NULL                                   'q:z'    DOMException(14) Namespace Error
element   NULL                                   ''       DOMException(5) Invalid Character Error
attribute NULL                                   ''       DOMException(5) Invalid Character Error
element   NULL                                   '1bad'   DOMException(5) Invalid Character Error
attribute NULL                                   '1bad'   DOMException(5) Invalid Character Error
element   NULL                                   ':z'     DOMException(5) Invalid Character Error
attribute NULL                                   ':z'     DOMException(5) Invalid Character Error
element   NULL                                   'z:'     DOMException(5) Invalid Character Error
attribute NULL                                   'z:'     DOMException(5) Invalid Character Error
element   NULL                                   'a:b:c'  DOMException(5) Invalid Character Error
attribute NULL                                   'a:b:c'  DOMException(5) Invalid Character Error
element   NULL                                   'xml:z'  DOMException(14) Namespace Error
attribute NULL                                   'xml:z'  DOMException(14) Namespace Error
element   NULL                                   'xmlns'  DOMException(14) Namespace Error
attribute NULL                                   'xmlns'  DOMException(14) Namespace Error
element   NULL                                   'xmlns:z' DOMException(14) Namespace Error
attribute NULL                                   'xmlns:z' DOMException(14) Namespace Error
element   ''                                     'z'      z local=z prefix=NULL uri=NULL
attribute ''                                     'z'      z local=z prefix=NULL uri=NULL
element   ''                                     'p:z'    DOMException(14) Namespace Error
attribute ''                                     'p:z'    DOMException(14) Namespace Error
element   ''                                     'q:z'    DOMException(14) Namespace Error
attribute ''                                     'q:z'    DOMException(14) Namespace Error
element   ''                                     ''       DOMException(5) Invalid Character Error
attribute ''                                     ''       DOMException(5) Invalid Character Error
element   ''                                     '1bad'   DOMException(5) Invalid Character Error
attribute ''                                     '1bad'   DOMException(5) Invalid Character Error
element   ''                                     ':z'     DOMException(5) Invalid Character Error
attribute ''                                     ':z'     DOMException(5) Invalid Character Error
element   ''                                     'z:'     DOMException(5) Invalid Character Error
attribute ''                                     'z:'     DOMException(5) Invalid Character Error
element   ''                                     'a:b:c'  DOMException(5) Invalid Character Error
attribute ''                                     'a:b:c'  DOMException(5) Invalid Character Error
element   ''                                     'xml:z'  DOMException(14) Namespace Error
attribute ''                                     'xml:z'  DOMException(14) Namespace Error
element   ''                                     'xmlns'  DOMException(14) Namespace Error
attribute ''                                     'xmlns'  DOMException(14) Namespace Error
element   ''                                     'xmlns:z' DOMException(14) Namespace Error
attribute ''                                     'xmlns:z' DOMException(14) Namespace Error
element   'urn:n'                                'z'      z local=z prefix=NULL uri='urn:n'
attribute 'urn:n'                                'z'      z local=z prefix=NULL uri='urn:n'
element   'urn:n'                                'p:z'    p:z local=z prefix='p' uri='urn:n'
attribute 'urn:n'                                'p:z'    p:z local=z prefix='p' uri='urn:n'
element   'urn:n'                                'q:z'    q:z local=z prefix='q' uri='urn:n'
attribute 'urn:n'                                'q:z'    q:z local=z prefix='q' uri='urn:n'
element   'urn:n'                                ''       DOMException(5) Invalid Character Error
attribute 'urn:n'                                ''       DOMException(5) Invalid Character Error
element   'urn:n'                                '1bad'   DOMException(5) Invalid Character Error
attribute 'urn:n'                                '1bad'   DOMException(5) Invalid Character Error
element   'urn:n'                                ':z'     DOMException(5) Invalid Character Error
attribute 'urn:n'                                ':z'     DOMException(5) Invalid Character Error
element   'urn:n'                                'z:'     DOMException(5) Invalid Character Error
attribute 'urn:n'                                'z:'     DOMException(5) Invalid Character Error
element   'urn:n'                                'a:b:c'  DOMException(5) Invalid Character Error
attribute 'urn:n'                                'a:b:c'  DOMException(5) Invalid Character Error
element   'urn:n'                                'xml:z'  DOMException(14) Namespace Error
attribute 'urn:n'                                'xml:z'  DOMException(14) Namespace Error
element   'urn:n'                                'xmlns'  DOMException(14) Namespace Error
attribute 'urn:n'                                'xmlns'  DOMException(14) Namespace Error
element   'urn:n'                                'xmlns:z' DOMException(14) Namespace Error
attribute 'urn:n'                                'xmlns:z' DOMException(14) Namespace Error
element   'urn:p'                                'z'      z local=z prefix=NULL uri='urn:p'
attribute 'urn:p'                                'z'      z local=z prefix=NULL uri='urn:p'
element   'urn:p'                                'p:z'    p:z local=z prefix='p' uri='urn:p'
attribute 'urn:p'                                'p:z'    p:z local=z prefix='p' uri='urn:p'
element   'urn:p'                                'q:z'    q:z local=z prefix='q' uri='urn:p'
attribute 'urn:p'                                'q:z'    q:z local=z prefix='q' uri='urn:p'
element   'urn:p'                                ''       DOMException(5) Invalid Character Error
attribute 'urn:p'                                ''       DOMException(5) Invalid Character Error
element   'urn:p'                                '1bad'   DOMException(5) Invalid Character Error
attribute 'urn:p'                                '1bad'   DOMException(5) Invalid Character Error
element   'urn:p'                                ':z'     DOMException(5) Invalid Character Error
attribute 'urn:p'                                ':z'     DOMException(5) Invalid Character Error
element   'urn:p'                                'z:'     DOMException(5) Invalid Character Error
attribute 'urn:p'                                'z:'     DOMException(5) Invalid Character Error
element   'urn:p'                                'a:b:c'  DOMException(5) Invalid Character Error
attribute 'urn:p'                                'a:b:c'  DOMException(5) Invalid Character Error
element   'urn:p'                                'xml:z'  DOMException(14) Namespace Error
attribute 'urn:p'                                'xml:z'  DOMException(14) Namespace Error
element   'urn:p'                                'xmlns'  DOMException(14) Namespace Error
attribute 'urn:p'                                'xmlns'  DOMException(14) Namespace Error
element   'urn:p'                                'xmlns:z' DOMException(14) Namespace Error
attribute 'urn:p'                                'xmlns:z' DOMException(14) Namespace Error
element   'http://www.w3.org/XML/1998/namespace' 'z'      z local=z prefix=NULL uri='http://www.w3.org/XML/1998/namespace'
attribute 'http://www.w3.org/XML/1998/namespace' 'z'      z local=z prefix=NULL uri='http://www.w3.org/XML/1998/namespace'
element   'http://www.w3.org/XML/1998/namespace' 'p:z'    p:z local=z prefix='p' uri='http://www.w3.org/XML/1998/namespace'
attribute 'http://www.w3.org/XML/1998/namespace' 'p:z'    p:z local=z prefix='p' uri='http://www.w3.org/XML/1998/namespace'
element   'http://www.w3.org/XML/1998/namespace' 'q:z'    q:z local=z prefix='q' uri='http://www.w3.org/XML/1998/namespace'
attribute 'http://www.w3.org/XML/1998/namespace' 'q:z'    q:z local=z prefix='q' uri='http://www.w3.org/XML/1998/namespace'
element   'http://www.w3.org/XML/1998/namespace' ''       DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/XML/1998/namespace' ''       DOMException(5) Invalid Character Error
element   'http://www.w3.org/XML/1998/namespace' '1bad'   DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/XML/1998/namespace' '1bad'   DOMException(5) Invalid Character Error
element   'http://www.w3.org/XML/1998/namespace' ':z'     DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/XML/1998/namespace' ':z'     DOMException(5) Invalid Character Error
element   'http://www.w3.org/XML/1998/namespace' 'z:'     DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/XML/1998/namespace' 'z:'     DOMException(5) Invalid Character Error
element   'http://www.w3.org/XML/1998/namespace' 'a:b:c'  DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/XML/1998/namespace' 'a:b:c'  DOMException(5) Invalid Character Error
element   'http://www.w3.org/XML/1998/namespace' 'xml:z'  xml:z local=z prefix='xml' uri='http://www.w3.org/XML/1998/namespace'
attribute 'http://www.w3.org/XML/1998/namespace' 'xml:z'  xml:z local=z prefix='xml' uri='http://www.w3.org/XML/1998/namespace'
element   'http://www.w3.org/XML/1998/namespace' 'xmlns'  DOMException(14) Namespace Error
attribute 'http://www.w3.org/XML/1998/namespace' 'xmlns'  DOMException(14) Namespace Error
element   'http://www.w3.org/XML/1998/namespace' 'xmlns:z' DOMException(14) Namespace Error
attribute 'http://www.w3.org/XML/1998/namespace' 'xmlns:z' DOMException(14) Namespace Error
element   'http://www.w3.org/2000/xmlns/'        'z'      DOMException(14) Namespace Error
attribute 'http://www.w3.org/2000/xmlns/'        'z'      DOMException(14) Namespace Error
element   'http://www.w3.org/2000/xmlns/'        'p:z'    DOMException(14) Namespace Error
attribute 'http://www.w3.org/2000/xmlns/'        'p:z'    DOMException(14) Namespace Error
element   'http://www.w3.org/2000/xmlns/'        'q:z'    DOMException(14) Namespace Error
attribute 'http://www.w3.org/2000/xmlns/'        'q:z'    DOMException(14) Namespace Error
element   'http://www.w3.org/2000/xmlns/'        ''       DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/2000/xmlns/'        ''       DOMException(5) Invalid Character Error
element   'http://www.w3.org/2000/xmlns/'        '1bad'   DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/2000/xmlns/'        '1bad'   DOMException(5) Invalid Character Error
element   'http://www.w3.org/2000/xmlns/'        ':z'     DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/2000/xmlns/'        ':z'     DOMException(5) Invalid Character Error
element   'http://www.w3.org/2000/xmlns/'        'z:'     DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/2000/xmlns/'        'z:'     DOMException(5) Invalid Character Error
element   'http://www.w3.org/2000/xmlns/'        'a:b:c'  DOMException(5) Invalid Character Error
attribute 'http://www.w3.org/2000/xmlns/'        'a:b:c'  DOMException(5) Invalid Character Error
element   'http://www.w3.org/2000/xmlns/'        'xml:z'  DOMException(14) Namespace Error
attribute 'http://www.w3.org/2000/xmlns/'        'xml:z'  DOMException(14) Namespace Error
element   'http://www.w3.org/2000/xmlns/'        'xmlns'  xmlns local=xmlns prefix=NULL uri='http://www.w3.org/2000/xmlns/'
attribute 'http://www.w3.org/2000/xmlns/'        'xmlns'  xmlns local=xmlns prefix=NULL uri='http://www.w3.org/2000/xmlns/'
element   'http://www.w3.org/2000/xmlns/'        'xmlns:z' xmlns:z local=z prefix='xmlns' uri='http://www.w3.org/2000/xmlns/'
attribute 'http://www.w3.org/2000/xmlns/'        'xmlns:z' xmlns:z local=z prefix='xmlns' uri='http://www.w3.org/2000/xmlns/'
child of element   urn:n  q:z  q='urn:n'  p='urn:p'  default=NULL     prefix(urn:n)='q'
child of element   urn:n  z    q=NULL     p='urn:p'  default='urn:n'  prefix(urn:n)=NULL
child of element   urn:p  p:z  q=NULL     p='urn:p'  default=NULL     prefix(urn:p)='p'
child of element   urn:n  p:z  q=NULL     p='urn:n'  default=NULL     prefix(urn:n)='p'
child of attribute urn:n  q:z  q=NULL     p='urn:p'  default=NULL     prefix(urn:n)=NULL
child of attribute urn:n  z    q=NULL     p='urn:p'  default=NULL     prefix(urn:n)=NULL
child of attribute urn:p  q:z  q=NULL     p='urn:p'  default=NULL     prefix(urn:p)='p'
child of attribute urn:n  p:z  q=NULL     p='urn:p'  default=NULL     prefix(urn:n)=NULL
collide: 13 An attribute with the given name in the given namespace already exists
itself: p:x local=x prefix='p' uri='urn:p'
detached: y local=y prefix=NULL uri=NULL
element: <r xmlns:p="urn:p"><b p:x="1" y="2"><k/></b></r>
bool(true)
bool(false)
bool(true)
bool(false)
bool(false)
Dom\Attr declares: isId,rename
