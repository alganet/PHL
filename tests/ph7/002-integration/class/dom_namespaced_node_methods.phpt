--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced DOM node declares php 8.4's twenty-two methods
--FILE--
<?php
/* php 8.4's `Dom\Node` carries twenty-two methods, and they are NOT DOMNode's
 * twenty-two: `hasAttributes` moved down to `Dom\Element`, DOM Level 1's
 * `isSupported` was dropped, every return type is real where the 2004 class's
 * is tentative, and four parameters gained the `?` the standard always had.
 *
 * The one that is php's own inconsistency, and the reason this test names it
 * twice: `insertBefore`'s $child is DECLARED with no default -- Reflection
 * reports TWO required parameters where DOMNode's reports one -- and the call
 * omits it anyway. Both faces are asked below. */
function sh($v) {
    if ($v === null) { return 'null'; }
    if (is_bool($v)) { return $v ? 'true' : 'false'; }
    if (is_object($v)) { return get_class($v) . '<' . ($v->nodeName ?? '?') . '>'; }
    if (is_string($v)) { return '"' . $v . '"'; }
    return var_export($v, true);
}
function t($label, callable $f) {
    try { echo $label, ' => ', sh($f()), "\n"; }
    catch (Throwable $e) { echo $label, ' !! ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
$xml = '<?xml version="1.0"?><r xmlns="urn:d" xmlns:p="urn:p">'
     . '<a id="x">t1</a><p:b/><!--c--><?pi d?></r>';
$d = Dom\XMLDocument::createFromString($xml);
$root = $d->documentElement;
$a = $root->firstChild;
$b = $a->nextSibling;

/* The declaration, whole: names, order, modifiers, types and defaults. */
$r = new ReflectionClass('Dom\Node');
foreach ($r->getMethods() as $m) {
    if ($m->class !== 'Dom\Node') { continue; }
    $ps = [];
    foreach ($m->getParameters() as $p) {
        $s = (string)($p->getType() ?? '');
        if ($p->isVariadic()) { $s .= ' ...'; }
        $s .= ' $' . $p->name;
        if ($p->isDefaultValueAvailable()) {
            $s .= ' = ' . str_replace(["\n", ' '], '', var_export($p->getDefaultValue(), true));
        }
        $ps[] = trim($s);
    }
    printf("%s %s(%s) : %s [req %d]\n",
        implode(' ', Reflection::getModifierNames($m->getModifiers())),
        $m->name, implode(', ', $ps),
        $m->getReturnType() ? (string)$m->getReturnType() : '<none>',
        $m->getNumberOfRequiredParameters());
}
/* The two DOMNode carries and this class does not. */
var_dump(method_exists('Dom\Node', 'hasAttributes'), method_exists('Dom\Node', 'isSupported'));

t('getRootNode', fn() => $a->getRootNode());
t('getRootNode(opts)', fn() => $a->getRootNode(['composed' => true]));
t('hasChildNodes root', fn() => $root->hasChildNodes());
t('hasChildNodes b', fn() => $b->hasChildNodes());
t('normalize', fn() => $root->normalize());
t('cloneNode', fn() => $a->cloneNode());
t('cloneNode deep children', fn() => $a->cloneNode(true)->childNodes->length);
t('isEqualNode self', fn() => $a->isEqualNode($a));
t('isEqualNode clone', fn() => $a->isEqualNode($a->cloneNode(true)));
t('isEqualNode null', fn() => $a->isEqualNode(null));
t('isEqualNode other', fn() => $a->isEqualNode($b));
t('isSameNode self', fn() => $a->isSameNode($a));
t('isSameNode null', fn() => $a->isSameNode(null));
t('compareDocumentPosition fwd', fn() => $a->compareDocumentPosition($b));
t('compareDocumentPosition rev', fn() => $b->compareDocumentPosition($a));
t('compareDocumentPosition down', fn() => $root->compareDocumentPosition($a));
t('compareDocumentPosition self', fn() => $a->compareDocumentPosition($a));
t('contains down', fn() => $root->contains($a));
t('contains up', fn() => $a->contains($root));
t('contains null', fn() => $a->contains(null));
/* The default declaration has no prefix, so the very URI lookupNamespaceURI(null)
 * hands back is one lookupPrefix() answers null for. */
t('lookupPrefix urn:p', fn() => $root->lookupPrefix('urn:p'));
t('lookupPrefix urn:d', fn() => $root->lookupPrefix('urn:d'));
t('lookupPrefix null', fn() => $root->lookupPrefix(null));
t('lookupNamespaceURI null', fn() => $root->lookupNamespaceURI(null));
t('lookupNamespaceURI p', fn() => $root->lookupNamespaceURI('p'));
t('lookupNamespaceURI absent', fn() => $root->lookupNamespaceURI('zz'));
t('isDefaultNamespace urn:d', fn() => $root->isDefaultNamespace('urn:d'));
t('isDefaultNamespace urn:p', fn() => $root->isDefaultNamespace('urn:p'));
t('isDefaultNamespace null', fn() => $root->isDefaultNamespace(null));
t('getLineNo', fn() => $a->getLineNo());
t('getNodePath', fn() => $a->getNodePath());
t('C14N', fn() => $a->C14N());
t('__sleep', fn() => $a->__sleep());
t('serialize', fn() => serialize($a));

/* Mutation, over nodes the parser produced. */
$d2 = Dom\XMLDocument::createFromString(
    '<?xml version="1.0"?><r><x/><y/><z><n/><m/><q/></z></r>');
$r2 = $d2->documentElement;
$x = $r2->firstChild; $y = $x->nextSibling; $zz = $y->nextSibling;
$n = $zz->firstChild; $m = $n->nextSibling; $q = $m->nextSibling;
t('appendChild', fn() => $r2->appendChild($n));
t('after append', fn() => $r2->C14N());
t('insertBefore', fn() => $r2->insertBefore($m, $y));
t('insertBefore one argument', fn() => $r2->insertBefore($q));
t('insertBefore explicit null', fn() => $r2->insertBefore($q, null));
t('after insert', fn() => $r2->C14N());
t('replaceChild', fn() => $r2->replaceChild($zz, $x));
t('removeChild', fn() => $r2->removeChild($y));
t('removeChild not a child', fn() => $r2->removeChild($x));
t('appendChild the document', fn() => $r2->appendChild($d2));
t('appendChild itself', fn() => $r2->appendChild($r2));
t('final', fn() => $r2->C14N());

/* The types, both directions: the two trees never meet. */
t('appendChild int', fn() => $r2->appendChild(1));
t('appendChild a DOMElement', fn() => $r2->appendChild((new DOMDocument)->createElement('L')));
t('compareDocumentPosition null', fn() => $a->compareDocumentPosition(null));
t('insertBefore no arguments', fn() => $r2->insertBefore());
t('DOMDocument::appendChild a Dom\Node', fn() => (new DOMDocument)->appendChild($a));
--EXPECT--
final private __construct() : <none> [req 0]
public getRootNode(array $options = array()) : Dom\Node [req 0]
public hasChildNodes() : bool [req 0]
public normalize() : void [req 0]
public cloneNode(bool $deep = false) : Dom\Node [req 0]
public isEqualNode(?Dom\Node $otherNode) : bool [req 1]
public isSameNode(?Dom\Node $otherNode) : bool [req 1]
public compareDocumentPosition(Dom\Node $other) : int [req 1]
public contains(?Dom\Node $other) : bool [req 1]
public lookupPrefix(?string $namespace) : ?string [req 1]
public lookupNamespaceURI(?string $prefix) : ?string [req 1]
public isDefaultNamespace(?string $namespace) : bool [req 1]
public insertBefore(Dom\Node $node, ?Dom\Node $child) : Dom\Node [req 2]
public appendChild(Dom\Node $node) : Dom\Node [req 1]
public replaceChild(Dom\Node $node, Dom\Node $child) : Dom\Node [req 2]
public removeChild(Dom\Node $child) : Dom\Node [req 1]
public getLineNo() : int [req 0]
public getNodePath() : string [req 0]
public C14N(bool $exclusive = false, bool $withComments = false, ?array $xpath = NULL, ?array $nsPrefixes = NULL) : string|false [req 0]
public C14NFile(string $uri, bool $exclusive = false, bool $withComments = false, ?array $xpath = NULL, ?array $nsPrefixes = NULL) : int|false [req 1]
public __sleep() : array [req 0]
public __wakeup() : void [req 0]
bool(false)
bool(false)
getRootNode => Dom\XMLDocument<#document>
getRootNode(opts) => Dom\XMLDocument<#document>
hasChildNodes root => true
hasChildNodes b => false
normalize => null
cloneNode => Dom\Element<a>
cloneNode deep children => 1
isEqualNode self => true
isEqualNode clone => true
isEqualNode null => false
isEqualNode other => false
isSameNode self => true
isSameNode null => false
compareDocumentPosition fwd => 4
compareDocumentPosition rev => 2
compareDocumentPosition down => 20
compareDocumentPosition self => 0
contains down => true
contains up => false
contains null => false
lookupPrefix urn:p => "p"
lookupPrefix urn:d => null
lookupPrefix null => null
lookupNamespaceURI null => "urn:d"
lookupNamespaceURI p => "urn:p"
lookupNamespaceURI absent => null
isDefaultNamespace urn:d => true
isDefaultNamespace urn:p => false
isDefaultNamespace null => false
getLineNo => 1
getNodePath => "/*/*[1]"
C14N => "<a xmlns="urn:d" xmlns:p="urn:p" id="x">t1</a>"
__sleep => __sleep !! Exception: Serialization of 'Dom\Element' is not allowed, unless serialization methods are implemented in a subclass
serialize => serialize !! Exception: Serialization of 'Dom\Element' is not allowed, unless serialization methods are implemented in a subclass
appendChild => Dom\Element<n>
after append => "<r><x></x><y></y><z><m></m><q></q></z><n></n></r>"
insertBefore => Dom\Element<m>
insertBefore one argument => Dom\Element<q>
insertBefore explicit null => Dom\Element<q>
after insert => "<r><x></x><m></m><y></y><z></z><n></n><q></q></r>"
replaceChild => Dom\Element<x>
removeChild => Dom\Element<y>
removeChild not a child => removeChild not a child !! DOMException: Not Found Error
appendChild the document => appendChild the document !! DOMException: Hierarchy Request Error
appendChild itself => appendChild itself !! DOMException: Hierarchy Request Error
final => "<r><z></z><m></m><n></n><q></q></r>"
appendChild int => appendChild int !! TypeError: Dom\Node::appendChild(): Argument #1 ($node) must be of type Dom\Node, int given
appendChild a DOMElement => appendChild a DOMElement !! TypeError: Dom\Node::appendChild(): Argument #1 ($node) must be of type Dom\Node, DOMElement given
compareDocumentPosition null => compareDocumentPosition null !! TypeError: Dom\Node::compareDocumentPosition(): Argument #1 ($other) must be of type Dom\Node, null given
insertBefore no arguments => insertBefore no arguments !! ArgumentCountError: Dom\Node::insertBefore() expects at least 1 argument, 0 given
DOMDocument::appendChild a Dom\Node => DOMDocument::appendChild a Dom\Node !! TypeError: DOMNode::appendChild(): Argument #1 ($node) must be of type DOMNode, Dom\Element given
