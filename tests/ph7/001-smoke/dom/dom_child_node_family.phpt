--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMChildNode: before/after/replaceWith/remove and the element-sibling properties
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "diag $no: ", str_replace("\n", '', $str), "\n"; return true; });
$dom_cn_doc = static function (string $xml = '<r/>'): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML($xml);
    return $d;
};
$dom_cn_try = static function (callable $fn) {
    try { $v = $fn(); echo '  -> ', is_object($v) ? get_class($v) : var_export($v, true), "\n"; }
    catch (Throwable $e) { printf("  %s(%d): %s\n", get_class($e), $e->getCode(), $e->getMessage()); }
};

// The interface, its method order, and who carries it: DOMElement directly,
// the character-data classes through DOMCharacterData -- and NOT the PI or
// the entity reference, php's own quirk against the WHATWG list.
var_dump(interface_exists('DOMChildNode'));
$r = new ReflectionClass('DOMChildNode');
var_dump(implode(',', array_map(fn($m) => $m->getName(), $r->getMethods())));
var_dump(in_array('DOMChildNode', class_implements('DOMElement'), true),
    in_array('DOMChildNode', class_implements('DOMCharacterData'), true),
    (new ReflectionClass('DOMText'))->implementsInterface('DOMChildNode'),
    in_array('DOMChildNode', (new ReflectionClass('DOMCdataSection'))->getInterfaceNames(), true),
    in_array('DOMParentNode', class_implements('DOMElement'), true));
$d = $dom_cn_doc('<r><?pi d?></r>');
$pi = $d->documentElement->firstChild;
$dom_cn_try(fn() => $pi->remove());
$dom_cn_try(fn() => $d->createEntityReference('amp')->remove());

// before/after: strings and nodes, in order, around the receiver.
$d = $dom_cn_doc('<r><b/></r>');
$b = $d->documentElement->firstChild;
var_dump($b->before('x', $d->createElement('a')));
$b->after($d->createElement('c'), 'y');
var_dump($d->saveXML($d->documentElement));
// ...on a text node, the methods come from DOMCharacterData.
$d = $dom_cn_doc('<r>t</r>');
$t = $d->documentElement->firstChild;
$t->before($d->createComment('pre'));
$t->after('post');
var_dump($d->saveXML($d->documentElement));
$dom_cn_try(fn() => $t->before(42));
// ...a comment can be removed, and the root element wrapped in new siblings.
$d = $dom_cn_doc('<!--lead--><r/>');
$d->documentElement->before($d->createComment('two'));
var_dump($d->saveXML());

// The viable sibling: a reference sibling is never a member of the argument
// set, computed before anything moves.
$d = $dom_cn_doc('<r><a/><b/><c/></r>');
$a = $d->documentElement->firstChild;
$c = $d->documentElement->lastChild;
$a->before($c);
var_dump($d->saveXML($d->documentElement));
$d = $dom_cn_doc('<r><a/><b/><c/><e/></r>');
$b = $d->documentElement->firstChild->nextSibling;
$b->after($b->nextSibling, $b);
var_dump($d->saveXML($d->documentElement));
$d = $dom_cn_doc('<r><a/><b/><c/></r>');
$b = $d->documentElement->firstChild->nextSibling;
$b->before($b);
$b->after($b);
var_dump($d->saveXML($d->documentElement));

// replaceWith: in place, itself (a no-op), its own next sibling, nothing.
$d = $dom_cn_doc('<r><a/><b/><c/></r>');
$b = $d->documentElement->firstChild->nextSibling;
$b->replaceWith('x', $d->createElement('n'));
var_dump($d->saveXML($d->documentElement));
$d = $dom_cn_doc('<r><a/><b/><c/></r>');
$b = $d->documentElement->firstChild->nextSibling;
$b->replaceWith($b);
var_dump($d->saveXML($d->documentElement));
$d = $dom_cn_doc('<r><a/><b/><c/></r>');
$b = $d->documentElement->firstChild->nextSibling;
$b->replaceWith($b->nextSibling);
var_dump($d->saveXML($d->documentElement));
$d = $dom_cn_doc('<r><a/><b/></r>');
$d->documentElement->firstChild->replaceWith();
var_dump($d->saveXML($d->documentElement));

// A node with NO parent: before/after/replaceWith return in silence -- even
// around an argument that could never be inserted -- where remove() is the
// Not Found refusal, in whichever mode the document is in.
$d = $dom_cn_doc();
$o = $d->createElement('o');
$o2 = $dom_cn_doc('<q><z/></q>');
var_dump($o->before('t'), $o->after($d->createAttribute('k')),
    $o->replaceWith($o2->documentElement->firstChild));
$dom_cn_try(fn() => $o->remove());
$d->strictErrorChecking = false;
$o3 = $d->createElement('o3');
$dom_cn_try(fn() => $o3->remove());
// ...but the TYPE screen still runs first, parent or no parent.
$dom_cn_try(fn() => $o->before(42));
// remove() takes no arguments at all.
$d = $dom_cn_doc('<r><a/></r>');
$dom_cn_try(fn() => $d->documentElement->firstChild->remove(1));
// ...and removing an attached node detaches it, wrapper intact.
$r2 = $d->documentElement;
var_dump($d->documentElement->firstChild->remove(), $d->saveXML($r2));
$r2->remove();
var_dump($d->saveXML(), $r2->parentNode, $r2->tagName);

// The same conversion machinery as the parent side: a refusal midway leaves
// the arguments already collected detached.
$d = $dom_cn_doc('<r><a/><b/><c/></r>');
$a = $d->documentElement->firstChild;
$b = $a->nextSibling;
$dom_cn_try(fn() => $b->before($a, $d->createAttribute('k')));
var_dump($d->saveXML($d->documentElement), $a->parentNode);
$d = $dom_cn_doc('<r><a/><b/><c/></r>');
$b = $d->documentElement->firstChild->nextSibling;
$dom_cn_try(fn() => $b->after($b, $d->documentElement));
var_dump($d->saveXML());
$d = $dom_cn_doc('<r><a/><b/></r>');
$dom_cn_try(fn() => $d->documentElement->lastChild->replaceWith('x', $d->documentElement));
var_dump($d->saveXML());
// ...wrong document is refused during that same collection.
$d = $dom_cn_doc('<r><a/><b/></r>');
$o2 = $dom_cn_doc('<q><z/></q>');
$dom_cn_try(fn() => $d->documentElement->firstChild->before('s', $o2->documentElement->firstChild));
var_dump($d->saveXML($d->documentElement));

// A fragment splices through before, and a child of a fragment has ChildNode
// too -- its parent is the fragment.
$d = $dom_cn_doc('<r><a/></r>');
$f = $d->createDocumentFragment();
$f->appendXML('<x/>y');
$d->documentElement->firstChild->before($f);
var_dump($d->saveXML($d->documentElement), $f->childNodes->length);
$f2 = $d->createDocumentFragment();
$f2->appendXML('<x/><y/>');
$f2->lastChild->before('mid');
var_dump($f2->childNodes->length);

// The two sibling properties: elements only, skipping every other node kind,
// on DOMElement and the character-data classes.
$d = $dom_cn_doc('<r><a/>t<!--c--><![CDATA[x]]><?p d?><b/>t2</r>');
$cd = $d->documentElement->childNodes->item(3);
var_dump(get_class($cd), $cd->previousElementSibling->tagName, $cd->nextElementSibling->tagName);
$a = $d->documentElement->firstChild;
var_dump($a->previousElementSibling, $a->nextElementSibling->tagName);
var_dump($a->nextElementSibling === $d->documentElement->lastChild->previousSibling);
// ...null at either end, on a detached node, and isset follows the value.
$d = $dom_cn_doc('<!--lead--><r/>');
var_dump($d->documentElement->previousElementSibling, $d->documentElement->nextElementSibling);
$o = $d->createElement('o');
var_dump($o->previousElementSibling, $o->nextElementSibling);
$d = $dom_cn_doc('<r><a/><b/></r>');
$a = $d->documentElement->firstChild;
var_dump(isset($a->nextElementSibling), isset($a->previousElementSibling));
// ...the write is the readonly refusal, under the INSTANCE's class name.
$dom_cn_try(function () use ($a) { $a->nextElementSibling = null; });
$d = $dom_cn_doc('<r><!--c--></r>');
$dom_cn_try(function () use ($d) { $d->documentElement->firstChild->previousElementSibling = null; });
// ...and a node kind php does not declare them on warns.
$d = $dom_cn_doc('<r k="1"><?pi d?></r>');
var_dump($d->documentElement->firstChild->previousElementSibling);
var_dump($d->documentElement->getAttributeNode('k')->nextElementSibling);
var_dump($d->previousElementSibling);
echo "done\n";
--EXPECT--
bool(true)
string(31) "remove,before,after,replaceWith"
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
  Error(0): Call to undefined method DOMProcessingInstruction::remove()
  Error(0): Call to undefined method DOMEntityReference::remove()
NULL
string(21) "<r>x<a/><b/><c/>y</r>"
string(22) "<r><!--pre-->tpost</r>"
  TypeError(0): DOMCharacterData::before(): Argument #1 must be of type DOMNode|string, int given
string(50) "<?xml version="1.0"?>
<!--lead-->
<!--two-->
<r/>
"
string(19) "<r><c/><a/><b/></r>"
string(23) "<r><a/><c/><b/><e/></r>"
string(19) "<r><a/><b/><c/></r>"
string(20) "<r><a/>x<n/><c/></r>"
string(19) "<r><a/><b/><c/></r>"
string(15) "<r><a/><c/></r>"
string(11) "<r><b/></r>"
NULL
NULL
NULL
  DOMException(8): Not Found Error
diag 2: DOMElement::remove(): Not Found Error
  -> NULL
  TypeError(0): DOMElement::before(): Argument #1 must be of type DOMNode|string, int given
  ArgumentCountError(0): DOMElement::remove() expects exactly 0 arguments, 1 given
NULL
string(4) "<r/>"
string(22) "<?xml version="1.0"?>
"
NULL
string(1) "r"
  DOMException(3): Hierarchy Request Error
string(15) "<r><b/><c/></r>"
NULL
  DOMException(3): Hierarchy Request Error
string(22) "<?xml version="1.0"?>
"
  DOMException(3): Hierarchy Request Error
string(22) "<?xml version="1.0"?>
"
  DOMException(4): Wrong Document Error
string(15) "<r><a/><b/></r>"
string(16) "<r><x/>y<a/></r>"
int(0)
int(3)
string(15) "DOMCdataSection"
string(1) "a"
string(1) "b"
NULL
string(1) "b"
bool(true)
NULL
NULL
NULL
NULL
bool(true)
bool(false)
  Error(0): Cannot modify readonly property DOMElement::$nextElementSibling
  Error(0): Cannot modify readonly property DOMComment::$previousElementSibling
diag 2: Undefined property: DOMProcessingInstruction::$previousElementSibling
NULL
diag 2: Undefined property: DOMAttr::$nextElementSibling
NULL
diag 2: Undefined property: DOMDocument::$previousElementSibling
NULL
done
