--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\ChildNode and Dom\ParentNode: the seven methods on every class that carries them
--FILE--
<?php
$dom_nsi_doc = static function (string $xml = '<r><a/><b/><c/></r>'): Dom\XMLDocument {
    return Dom\XMLDocument::createFromString($xml);
};
$dom_nsi_try = static function (string $label, callable $fn): void {
    try { printf("%s: %s\n", $label, var_export($fn(), true)); }
    catch (Throwable $e) { printf("%s: %s(%d): %s\n", $label, get_class($e), $e->getCode(), $e->getMessage()); }
};

// php declares the WHATWG mixins' methods ON each implementing class, where the
// two interfaces only state them abstract. Five classes carry them, and
// Dom\Element is the only one with both sides.
foreach (['Dom\Element', 'Dom\CharacterData', 'Dom\DocumentType', 'Dom\DocumentFragment',
          'Dom\Document', 'Dom\Text', 'Dom\XMLDocument'] as $dom_nsi_cls) {
    $dom_nsi_r = new ReflectionClass($dom_nsi_cls);
    $dom_nsi_own = [];
    foreach ($dom_nsi_r->getMethods() as $dom_nsi_m) {
        if ($dom_nsi_m->getDeclaringClass()->getName() !== $dom_nsi_cls) { continue; }
        if (!in_array($dom_nsi_m->getName(), ['remove', 'before', 'after', 'replaceWith',
                                              'append', 'prepend', 'replaceChildren'], true)) { continue; }
        $dom_nsi_p = [];
        foreach ($dom_nsi_m->getParameters() as $dom_nsi_arg) {
            $dom_nsi_p[] = ($dom_nsi_arg->getType() ?? '') . ($dom_nsi_arg->isVariadic() ? ' ...' : ' ') . '$' . $dom_nsi_arg->getName();
        }
        $dom_nsi_own[] = sprintf('%s(%s):%s', $dom_nsi_m->getName(), implode(', ', $dom_nsi_p),
                                 (string) $dom_nsi_m->getReturnType());
    }
    printf("decl %s: %s\n", $dom_nsi_cls, implode(' ', $dom_nsi_own));
}

// Each of the seven, on an element.
$dom_nsi_try('remove', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->firstChild->remove(); return $e->C14N(); });
$dom_nsi_try('before strings', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->firstChild->before('x', 'y'); return $e->C14N(); });
$dom_nsi_try('before none', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->firstChild->before(); return $e->C14N(); });
$dom_nsi_try('after node', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->firstChild->after($e->lastChild); return $e->C14N(); });
$dom_nsi_try('after self', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $a = $e->firstChild; $a->after($a); return $e->C14N(); });
$dom_nsi_try('replaceWith', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->firstChild->replaceWith('z'); return $e->C14N(); });
$dom_nsi_try('replaceWith self', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $a = $e->firstChild; $a->replaceWith($a); return $e->C14N(); });
$dom_nsi_try('append', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->append('t'); return $e->C14N(); });
$dom_nsi_try('append reorders', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->append($e->firstChild); return $e->C14N(); });
$dom_nsi_try('prepend', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->prepend($e->lastChild, 'q'); return $e->C14N(); });
$dom_nsi_try('replaceChildren', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->replaceChildren('only'); return $e->C14N(); });
$dom_nsi_try('replaceChildren none', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->replaceChildren(); return $e->C14N(); });

// The character-data side, over every kind that inherits it.
$dom_nsi_try('text remove', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r>hi<a/></r>'); $e = $d->documentElement; $e->firstChild->remove(); return $e->C14N(); });
$dom_nsi_try('text before', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r>hi<a/></r>'); $e = $d->documentElement; $e->firstChild->before('q'); return $e->C14N(); });
$dom_nsi_try('comment replaceWith', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r><!--c--><a/></r>'); $e = $d->documentElement; $e->firstChild->replaceWith('q'); return $e->C14N(); });
$dom_nsi_try('cdata remove', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r><![CDATA[x]]><a/></r>'); $e = $d->documentElement; $e->firstChild->remove(); return $e->C14N(); });
$dom_nsi_try('pi after', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r><?pi v?><a/></r>'); $e = $d->documentElement; $e->firstChild->after('q'); return $e->C14N(); });

// The refusals the shared bodies already carried, now reachable under these names.
$dom_nsi_try('append self', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $e = $d->documentElement; $e->append($e); return $e->C14N(); });
$dom_nsi_try('append cross-document', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $o = $dom_nsi_doc('<o><z/></o>'); $d->documentElement->append($o->documentElement->firstChild); return 'ok'; });
$dom_nsi_try('append ancestor', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r><a><b/></a></r>'); $e = $d->documentElement; $e->firstChild->append($e); return 'ok'; });

// A DOCUMENT parent carries rules the 2004 tree does not: php's four sentences.
$dom_nsi_try('doc appends its own root', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $d->append($d->documentElement); return 'ok'; });
$dom_nsi_try('doc appends a string', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $d->append('s'); return 'ok'; });
$dom_nsi_try('doc appends cdata', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r><![CDATA[x]]></r>'); $d->append($d->documentElement->firstChild); return 'ok'; });
$dom_nsi_try('doc appends a comment', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r><!--c--></r>'); $d->append($d->documentElement->firstChild); return count(iterator_to_array($d->childNodes)); });
$dom_nsi_try('doc appends a second doctype', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<!DOCTYPE r><r/>'); $d->append($d->doctype); return 'ok'; });
$dom_nsi_try('root before a string', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $d->documentElement->before('x'); return 'ok'; });
$dom_nsi_try('root before a comment', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r><!--c--></r>'); $d->documentElement->before($d->documentElement->firstChild); return count(iterator_to_array($d->childNodes)); });
$dom_nsi_try('root replaceWith itself', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $d->documentElement->replaceWith($d->documentElement); return 'ok'; });
$dom_nsi_try('doc replaceChildren its root', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $d->replaceChildren($d->documentElement); return 'ok'; });
$dom_nsi_try('doc replaceChildren nothing', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $d->replaceChildren(); return $d->documentElement === null ? 'empty' : 'kept'; });
// ...and the state-dependent half: a rootless document takes ONE element, and a
// document type may never end up after one.
$dom_nsi_pull = static function (Dom\XMLDocument $d): Dom\Node {
    $n = $d->documentElement->firstChild;
    $n->parentNode->removeChild($n);
    $d->removeChild($d->documentElement);
    return $n;
};
$dom_nsi_try('rootless doc takes one element', function () use ($dom_nsi_doc, $dom_nsi_pull) { $d = $dom_nsi_doc('<r><e/></r>'); $e = $dom_nsi_pull($d); $d->append($e); return $d->documentElement->nodeName; });
$dom_nsi_try('rootless doc refuses two', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r><e/><f/></r>'); $e = $d->documentElement->firstChild; $f = $d->documentElement->lastChild; $e->parentNode->removeChild($e); $f->parentNode->removeChild($f); $d->removeChild($d->documentElement); $d->append($e, $f); return 'ok'; });
$dom_nsi_try('rootless doc refuses text', function () use ($dom_nsi_doc, $dom_nsi_pull) { $d = $dom_nsi_doc('<r>t</r>'); $t = $dom_nsi_pull($d); $d->append($t); return 'ok'; });
$dom_nsi_try('element before a doctype', function () use ($dom_nsi_doc, $dom_nsi_pull) { $d = $dom_nsi_doc('<!DOCTYPE r><r><e/></r>'); $e = $dom_nsi_pull($d); $d->prepend($e); return 'ok'; });
$dom_nsi_try('element after a doctype', function () use ($dom_nsi_doc, $dom_nsi_pull) { $d = $dom_nsi_doc('<!DOCTYPE r><r><e/></r>'); $e = $dom_nsi_pull($d); $d->doctype->after($e); return $d->documentElement->nodeName; });
$dom_nsi_try('doctype after an element', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<!DOCTYPE r><r/>'); $t = $d->doctype; $d->removeChild($t); $d->documentElement->after($t); return 'ok'; });
$dom_nsi_try('doctype before an element', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<!DOCTYPE r><r/>'); $t = $d->doctype; $d->removeChild($t); $d->documentElement->before($t); return $d->doctype === null ? 'null' : 'set'; });

// The screen: `Dom\Node|string` here, `DOMNode|string` there, and the two trees
// never take each other's nodes.
foreach ([1, 1.5, null, true, [], new stdClass] as $dom_nsi_bad) {
    $dom_nsi_try('screen', function () use ($dom_nsi_doc, $dom_nsi_bad) { $d = $dom_nsi_doc(); $d->documentElement->firstChild->before($dom_nsi_bad); });
}
$dom_nsi_try('screen names the declarer (chardata)', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<r>hi</r>'); $d->documentElement->firstChild->before(1); });
$dom_nsi_try('screen names the declarer (document)', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $d->append(1); });
$dom_nsi_try('screen names the declarer (doctype)', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc('<!DOCTYPE r><r/>'); $d->doctype->after(1); });
$dom_nsi_try('screen refuses the second argument', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $d->documentElement->append('ok', 7); });
$dom_nsi_try('a DOMNode is not a Dom\Node', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $l = new DOMDocument; $l->loadXML('<x/>'); $d->documentElement->append($l->documentElement); });
$dom_nsi_try('a Dom\Node is not a DOMNode', function () use ($dom_nsi_doc) { $d = $dom_nsi_doc(); $l = new DOMDocument; $l->loadXML('<x><y/></x>'); $l->documentElement->firstChild->before($d->documentElement); });

// ...and the 2004 tree keeps its own, looser, document rules.
$dom_nsi_try('DOMDocument still takes a second root', function () { $d = new DOMDocument; $d->loadXML('<r/>'); $d->append($d->createElement('q')); return $d->saveXML(); });
$dom_nsi_try('DOMDocument still takes bare text', function () { $d = new DOMDocument; $d->loadXML('<r/>'); $d->documentElement->before('x'); return $d->saveXML(); });
?>
--EXPECT--
decl Dom\Element: remove():void before(Dom\Node|string ...$nodes):void after(Dom\Node|string ...$nodes):void replaceWith(Dom\Node|string ...$nodes):void append(Dom\Node|string ...$nodes):void prepend(Dom\Node|string ...$nodes):void replaceChildren(Dom\Node|string ...$nodes):void
decl Dom\CharacterData: remove():void before(Dom\Node|string ...$nodes):void after(Dom\Node|string ...$nodes):void replaceWith(Dom\Node|string ...$nodes):void
decl Dom\DocumentType: remove():void before(Dom\Node|string ...$nodes):void after(Dom\Node|string ...$nodes):void replaceWith(Dom\Node|string ...$nodes):void
decl Dom\DocumentFragment: append(Dom\Node|string ...$nodes):void prepend(Dom\Node|string ...$nodes):void replaceChildren(Dom\Node|string ...$nodes):void
decl Dom\Document: append(Dom\Node|string ...$nodes):void prepend(Dom\Node|string ...$nodes):void replaceChildren(Dom\Node|string ...$nodes):void
decl Dom\Text: 
decl Dom\XMLDocument: 
remove: '<r><b></b><c></c></r>'
before strings: '<r>xy<a></a><b></b><c></c></r>'
before none: '<r><a></a><b></b><c></c></r>'
after node: '<r><a></a><c></c><b></b></r>'
after self: '<r><a></a><b></b><c></c></r>'
replaceWith: '<r>z<b></b><c></c></r>'
replaceWith self: '<r><a></a><b></b><c></c></r>'
append: '<r><a></a><b></b><c></c>t</r>'
append reorders: '<r><b></b><c></c><a></a></r>'
prepend: '<r><c></c>q<a></a><b></b></r>'
replaceChildren: '<r>only</r>'
replaceChildren none: '<r></r>'
text remove: '<r><a></a></r>'
text before: '<r>qhi<a></a></r>'
comment replaceWith: '<r>q<a></a></r>'
cdata remove: '<r><a></a></r>'
pi after: '<r><?pi v?>q<a></a></r>'
append self: DOMException(3): Hierarchy Request Error
append cross-document: DOMException(4): Wrong Document Error
append ancestor: DOMException(3): Hierarchy Request Error
doc appends its own root: DOMException(3): Cannot have more than one element child in a document
doc appends a string: DOMException(3): Cannot insert text as a child of a document
doc appends cdata: DOMException(3): Cannot insert text as a child of a document
doc appends a comment: 2
doc appends a second doctype: DOMException(3): Cannot have more than one document type
root before a string: DOMException(3): Cannot insert text as a child of a document
root before a comment: 2
root replaceWith itself: DOMException(3): Cannot have more than one element child in a document
doc replaceChildren its root: DOMException(3): Cannot have more than one element child in a document
doc replaceChildren nothing: 'empty'
rootless doc takes one element: 'e'
rootless doc refuses two: DOMException(3): Cannot have more than one element child in a document
rootless doc refuses text: DOMException(3): Cannot insert text as a child of a document
element before a doctype: DOMException(3): Document types must be the first child in a document
element after a doctype: 'e'
doctype after an element: DOMException(3): Document types must be the first child in a document
doctype before an element: 'set'
screen: TypeError(0): Dom\Element::before(): Argument #1 must be of type Dom\Node|string, int given
screen: TypeError(0): Dom\Element::before(): Argument #1 must be of type Dom\Node|string, float given
screen: TypeError(0): Dom\Element::before(): Argument #1 must be of type Dom\Node|string, null given
screen: TypeError(0): Dom\Element::before(): Argument #1 must be of type Dom\Node|string, bool given
screen: TypeError(0): Dom\Element::before(): Argument #1 must be of type Dom\Node|string, array given
screen: TypeError(0): Dom\Element::before(): Argument #1 must be of type Dom\Node|string, stdClass given
screen names the declarer (chardata): TypeError(0): Dom\CharacterData::before(): Argument #1 must be of type Dom\Node|string, int given
screen names the declarer (document): TypeError(0): Dom\Document::append(): Argument #1 must be of type Dom\Node|string, int given
screen names the declarer (doctype): TypeError(0): Dom\DocumentType::after(): Argument #1 must be of type Dom\Node|string, int given
screen refuses the second argument: TypeError(0): Dom\Element::append(): Argument #2 must be of type Dom\Node|string, int given
a DOMNode is not a Dom\Node: TypeError(0): Dom\Element::append(): Argument #1 must be of type Dom\Node|string, DOMElement given
a Dom\Node is not a DOMNode: TypeError(0): DOMElement::before(): Argument #1 must be of type DOMNode|string, Dom\Element given
DOMDocument still takes a second root: '<?xml version="1.0"?>
<r/>
<q/>
'
DOMDocument still takes bare text: '<?xml version="1.0"?>
x
<r/>
'
