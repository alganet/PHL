--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMParentNode: append/prepend/replaceChildren and the element-child properties
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "diag $no: ", str_replace("\n", '', $str), "\n"; return true; });
$dom_pn_doc = static function (string $xml = '<r/>'): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML($xml);
    return $d;
};
$dom_pn_try = static function (callable $fn) {
    try { $v = $fn(); echo '  -> ', is_object($v) ? get_class($v) : var_export($v, true), "\n"; }
    catch (Throwable $e) { printf("  %s(%d): %s\n", get_class($e), $e->getCode(), $e->getMessage()); }
};

// The interface and its implementers. php declares DOMParentNode on exactly
// three classes, and the methods are real (non-tentative) void with one
// untyped variadic.
var_dump(interface_exists('DOMParentNode'));
foreach (['DOMDocument', 'DOMElement', 'DOMDocumentFragment'] as $c) {
    var_dump(in_array('DOMParentNode', class_implements($c), true));
}
$m = new ReflectionMethod('DOMElement', 'append');
var_dump($m->getParameters()[0]->isVariadic(), $m->getParameters()[0]->hasType(),
    (string)$m->getReturnType());
$d = $dom_pn_doc();
var_dump($d->documentElement instanceof DOMParentNode, $d instanceof DOMParentNode);

// append/prepend: a string becomes a text node, arguments keep their order,
// and the answer is void.
$d = $dom_pn_doc('<root><a/>mid</root>');
$r = $d->documentElement;
var_dump($r->append('t1', $d->createElement('new'), 't2'));
$r->prepend('p0', $d->createElement('pre'));
var_dump($d->saveXML($r));
// ...an empty string still inserts a (serialized-empty) text node.
$d = $dom_pn_doc();
$d->documentElement->append('');
var_dump($d->documentElement->childNodes->length, $d->saveXML($d->documentElement));

// A fragment splices; an EMPTY fragment is a silent no-op here, unlike
// appendChild's warning.
$d = $dom_pn_doc();
$f = $d->createDocumentFragment();
$f->appendXML('<x/>y<z/>');
$d->documentElement->append($f);
$d->documentElement->append($d->createDocumentFragment());
var_dump($d->saveXML($d->documentElement), $f->childNodes->length);
// ...and the fragment's own append works on the fragment.
$f2 = $d->createDocumentFragment();
$f2->append('t', $d->createElement('k'));
var_dump($f2->childNodes->length);

// The type screen: DOMNode|string only, nothing coerces, the WHOLE list is
// checked before anything moves, and the position is named without a
// parameter name.
class DomPnStringable { public function __toString(): string { return 'S'; } }
$d = $dom_pn_doc();
$r = $d->documentElement;
foreach ([42, 1.5, null, true, [], new stdClass, new DomPnStringable] as $bad) {
    $dom_pn_try(fn() => $r->append($bad));
}
$dom_pn_try(fn() => $r->append('ok', 42));
$el = $d->createElement('mine');
$dom_pn_try(fn() => $r->append($el, 3.5));
var_dump($d->saveXML($r));   // still <r/>: nothing was inserted

// The validity screen: wrong document BEFORE an unlinkable kind, first
// invalid argument wins, and a document, an attribute or an ancestor is the
// Hierarchy refusal.
$d = $dom_pn_doc();
$o = $dom_pn_doc('<q><z/></q>');
$r = $d->documentElement;
$dom_pn_try(fn() => $r->append($r));
$dom_pn_try(fn() => $r->append($d));
$dom_pn_try(fn() => $r->append($d->createAttribute('k')));
$dom_pn_try(fn() => $r->append($o->documentElement->firstChild));
$dom_pn_try(fn() => $r->append($o->createAttribute('k')));
$dom_pn_try(fn() => $r->append($d->createElement('mine'), $o->documentElement->firstChild));
var_dump($d->saveXML($r));
$d = $dom_pn_doc('<r><mid><leaf/></mid></r>');
$mid = $d->documentElement->firstChild;
$dom_pn_try(fn() => $mid->append($d->documentElement));
$f = $d->createDocumentFragment();
$dom_pn_try(fn() => $f->append($f));

// php's one-argument SHORTCUT against its multi-argument conversion, both
// measured: one argument is handed through whole -- a self-append moves
// nothing -- while two or more really are collected into a fragment, so a
// refusal midway leaves every argument already collected DETACHED, alive for
// whatever variable still holds it.
$d = $dom_pn_doc('<r><b/></r>');
$dom_pn_try(fn() => $d->documentElement->append($d->documentElement));
var_dump($d->saveXML());   // one argument: the tree is intact
$d = $dom_pn_doc('<r><a/><b/></r>');
$a = $d->documentElement->firstChild;
$b = $d->documentElement->lastChild;
$dom_pn_try(fn() => $b->append($a, $b));
var_dump($d->saveXML($d->documentElement), $a->parentNode, $a->nodeName);
$d = $dom_pn_doc('<r><a/><b/></r>');
$a = $d->documentElement->firstChild;
$dom_pn_try(fn() => $d->documentElement->lastChild->append($a, $d->createAttribute('k')));
var_dump($d->saveXML($d->documentElement));
// ...an uninitialized DOM shell: a silent no-op alone, Invalid State in a list.
$shell = (new ReflectionClass('DOMText'))->newInstanceWithoutConstructor();
$d = $dom_pn_doc();
$dom_pn_try(fn() => $d->documentElement->append($shell));
$dom_pn_try(fn() => $d->documentElement->append('a', $shell));
var_dump($d->saveXML($d->documentElement));

// What php does NOT refuse on a document: a second root element, bare text, a
// comment or a PI -- the document written may not be well-formed XML, and that
// is php's own answer.
$d = $dom_pn_doc();
$d->append($d->createElement('r2'));
$d->append('bare');
$d->prepend($d->createComment('c'));
var_dump($d->saveXML());
$d = new DOMDocument;
$d->append($d->createElement('made'));
var_dump($d->saveXML());
// ...but a CDATA section on a document is still allowed through quietly too.
$d = $dom_pn_doc();
$dom_pn_try(fn() => $d->append($d->createCDATASection('cd')));

// Conversion-first: a set member leaves the tree before the insertion point is
// read, so prepending the first child lands it before the SECOND.
$d = $dom_pn_doc('<r><a/><b/></r>');
$a = $d->documentElement->firstChild;
$d->documentElement->prepend($a, 'x');
var_dump($d->saveXML($d->documentElement));
// ...and a duplicated argument inserts once.
$d = $dom_pn_doc('<r><a/><b/></r>');
$a = $d->documentElement->firstChild;
$d->documentElement->append($a, $a);
var_dump($d->saveXML($d->documentElement));

// Identity survives the move.
$d = $dom_pn_doc('<r><a/><b/></r>');
$a = $d->documentElement->firstChild;
$b = $d->documentElement->lastChild;
$b->append($a);
var_dump($b->firstChild === $a, $a->parentNode === $b);

// Namespace reconcile runs for a moved node.
$d = $dom_pn_doc('<r xmlns:p="urn:a"><p:b k="1"/><t/></r>');
$pb = $d->documentElement->firstChild;
$d->documentElement->lastChild->append($pb);
var_dump($d->saveXML($d->documentElement));

// replaceChildren: replaces, empties on no arguments, keeps the old children
// when an argument is refused, and reuses an existing child.
$d = $dom_pn_doc('<r><a/><b/>t</r>');
$d->documentElement->replaceChildren('new', $d->createElement('k'));
var_dump($d->saveXML($d->documentElement));
$d->documentElement->replaceChildren();
var_dump($d->saveXML($d->documentElement));
$d = $dom_pn_doc('<r><a/><b/></r>');
$dom_pn_try(fn() => $d->documentElement->replaceChildren('new', 42));
var_dump($d->saveXML($d->documentElement));
$a = $d->documentElement->firstChild;
$d->documentElement->replaceChildren($a, 'x');
var_dump($d->saveXML($d->documentElement));
$dom_pn_try(fn() => $d->documentElement->replaceChildren($d->documentElement));
$d = $dom_pn_doc('<!--c--><r><a/></r>');
$d->replaceChildren($d->createElement('n'));
var_dump($d->saveXML());
// ...a child a variable still holds survives the drop, detached.
$d = $dom_pn_doc('<r><keep>t</keep></r>');
$keep = $d->documentElement->firstChild;
$d->documentElement->replaceChildren('gone');
var_dump($keep->parentNode, $keep->textContent);

// The non-strict mode: the same sentence as a warning under the METHOD's
// name, and nothing -- these are void -- is answered or inserted.
$d = $dom_pn_doc();
$d->strictErrorChecking = false;
$dom_pn_try(fn() => $d->documentElement->append($d->createAttribute('k')));
$o = $dom_pn_doc('<q><z/></q>');
$dom_pn_try(fn() => $d->documentElement->prepend($o->documentElement->firstChild));
var_dump($d->saveXML($d->documentElement));

// The three properties, on all three implementers -- and the null answers.
$d = $dom_pn_doc('<r>t1<a/>t2<b/>t3<c/>t4</r>');
$r = $d->documentElement;
var_dump($r->firstElementChild->tagName, $r->lastElementChild->tagName, $r->childElementCount);
var_dump($r->firstElementChild === $r->childNodes->item(1));
$d = $dom_pn_doc('<r>plain</r>');
var_dump($d->documentElement->firstElementChild, $d->documentElement->lastElementChild,
    $d->documentElement->childElementCount);
$d = $dom_pn_doc('<!--c--><r/>');
var_dump($d->firstElementChild->tagName, $d->lastElementChild->tagName, $d->childElementCount);
$f = $d->createDocumentFragment();
$f->appendXML('t<a/><b/>');
var_dump($f->firstElementChild->tagName, $f->lastElementChild->tagName, $f->childElementCount);
// isset: a declared name reads back non-null or not.
$d = $dom_pn_doc('<r><a/></r>');
var_dump(isset($d->documentElement->firstElementChild), isset($d->childElementCount),
    isset($d->createDocumentFragment()->firstElementChild));
// The write refusal names the property as readonly.
$dom_pn_try(function () use ($d) { $d->documentElement->firstElementChild = null; });
$dom_pn_try(function () use ($d) { $d->childElementCount = 5; });
// ...and a node kind OUTSIDE the three implementers does not declare them:
// the read is the Undefined property warning, null, isset false.
$d = $dom_pn_doc('<r>t</r>');
$t = $d->documentElement->firstChild;
var_dump($t->childElementCount, isset($t->childElementCount));
$at = $d->createAttribute('k');
var_dump($at->firstElementChild);
echo "done\n";
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
string(4) "void"
bool(true)
bool(true)
NULL
string(38) "<root>p0<pre/><a/>midt1<new/>t2</root>"
int(1)
string(7) "<r></r>"
string(16) "<r><x/>y<z/></r>"
int(0)
int(2)
  TypeError(0): DOMElement::append(): Argument #1 must be of type DOMNode|string, int given
  TypeError(0): DOMElement::append(): Argument #1 must be of type DOMNode|string, float given
  TypeError(0): DOMElement::append(): Argument #1 must be of type DOMNode|string, null given
  TypeError(0): DOMElement::append(): Argument #1 must be of type DOMNode|string, bool given
  TypeError(0): DOMElement::append(): Argument #1 must be of type DOMNode|string, array given
  TypeError(0): DOMElement::append(): Argument #1 must be of type DOMNode|string, stdClass given
  TypeError(0): DOMElement::append(): Argument #1 must be of type DOMNode|string, DomPnStringable given
  TypeError(0): DOMElement::append(): Argument #2 must be of type DOMNode|string, int given
  TypeError(0): DOMElement::append(): Argument #2 must be of type DOMNode|string, float given
string(4) "<r/>"
  DOMException(3): Hierarchy Request Error
  DOMException(3): Hierarchy Request Error
  DOMException(3): Hierarchy Request Error
  DOMException(4): Wrong Document Error
  DOMException(4): Wrong Document Error
  DOMException(4): Wrong Document Error
string(4) "<r/>"
  DOMException(3): Hierarchy Request Error
  DOMException(3): Hierarchy Request Error
  DOMException(3): Hierarchy Request Error
string(34) "<?xml version="1.0"?>
<r><b/></r>
"
  DOMException(3): Hierarchy Request Error
string(4) "<r/>"
NULL
string(1) "a"
  DOMException(3): Hierarchy Request Error
string(11) "<r><b/></r>"
  -> NULL
  DOMException(11): Invalid State Error
string(4) "<r/>"
string(47) "<?xml version="1.0"?>
<!--c-->
<r/>
<r2/>
bare
"
string(30) "<?xml version="1.0"?>
<made/>
"
  -> NULL
string(16) "<r><a/>x<b/></r>"
string(15) "<r><b/><a/></r>"
bool(true)
bool(true)
string(42) "<r xmlns:p="urn:a"><t><p:b k="1"/></t></r>"
string(14) "<r>new<k/></r>"
string(4) "<r/>"
  TypeError(0): DOMElement::replaceChildren(): Argument #2 must be of type DOMNode|string, int given
string(15) "<r><a/><b/></r>"
string(12) "<r><a/>x</r>"
  DOMException(3): Hierarchy Request Error
string(27) "<?xml version="1.0"?>
<n/>
"
NULL
string(1) "t"
diag 2: DOMElement::append(): Hierarchy Request Error
  -> NULL
diag 2: DOMElement::prepend(): Wrong Document Error
  -> NULL
string(4) "<r/>"
string(1) "a"
string(1) "c"
int(3)
bool(true)
NULL
NULL
int(0)
string(1) "r"
string(1) "r"
int(1)
string(1) "a"
string(1) "b"
int(2)
bool(true)
bool(true)
bool(false)
  Error(0): Cannot modify readonly property DOMElement::$firstElementChild
  Error(0): Cannot modify readonly property DOMDocument::$childElementCount
diag 2: Undefined property: DOMText::$childElementCount
NULL
bool(false)
diag 2: Undefined property: DOMAttr::$firstElementChild
NULL
done
