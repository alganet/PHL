--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parent node lists its element children as one remembered live collection
--FILE--
<?php
/* php 8.4's `children` is the element-only half of `childNodes`, declared on
 * the three classes that can have element children -- the document, the
 * element and the fragment -- and it differs from its sibling `childNodes` in
 * three ways that are each observable:
 *
 *  - it is a `Dom\HTMLCollection` and not a `Dom\NodeList`, so it answers
 *    namedItem() as well as item();
 *  - it skips every child that is not an element, so the text, comment and
 *    processing-instruction children `childNodes` counts are not in it, and
 *    the document's own `children` holds the document element alone;
 *  - it is REMEMBERED on the node: `$e->children === $e->children` is true,
 *    where `$e->childNodes === $e->childNodes` is false and every
 *    getElementsByTagName() call mints a fresh list.
 *
 * Being remembered does not make it a snapshot -- it is still a live view, and
 * a child appended after the first read is in the collection already held.
 *
 * php states the whole ParentNode group before each class's own names, and
 * `children` at the head of it, which Reflection's property listing and the
 * debug walk both show. The write and the unset are the two refusals a
 * declared-but-unwritable name carries. */
$d = Dom\XMLDocument::createFromString('<r><a id="x"/>t<b/><!--c--><?pi ?><c/></r>');
$r = $d->documentElement;
$ch = $r->children;

echo get_class($ch), ' len=', $ch->length, ' count=', count($ch),
     ' childNodes=', $r->childNodes->length, "\n";
foreach ($ch as $k => $n) {
    echo "  [$k] ", $n->nodeName, "\n";
}
echo 'item(0)=', $ch->item(0)?->nodeName,
     ' item(9)=', var_export($ch->item(9), true),
     ' named(x)=', $ch->namedItem('x')?->nodeName,
     ' dim[1]=', $ch[1]?->nodeName, "\n";

$r->appendChild($d->createElement('d'));
echo 'live=', $ch->length, "\n";

echo 'doc=', $d->children->length, ':', $d->children->item(0)?->nodeName,
     ' empty=', $d->createElement('e')->children->length, "\n";
$f = $d->createDocumentFragment();
$f->appendChild($d->createElement('q'));
$f->appendChild($d->createTextNode('t'));
echo 'frag=', $f->children->length, ':', $f->children->item(0)?->nodeName, "\n";

echo 'same=', var_export($r->children === $r->children, true),
     ' childNodes=', var_export($r->childNodes === $r->childNodes, true),
     ' gebtn=', var_export($r->getElementsByTagName('a') === $r->getElementsByTagName('a'), true),
     "\n";

echo "== declared ==\n";
foreach (['Dom\Element', 'Dom\Document', 'Dom\DocumentFragment'] as $c) {
    $names = [];
    foreach ((new ReflectionClass($c))->getProperties() as $p) {
        if ($p->getDeclaringClass()->getName() === $c) {
            $names[] = $p->getName();
        }
    }
    echo $c, ': ', implode(' ', array_slice($names, 0, 5)), "\n";
}
echo (new ReflectionClass('Dom\Element'))->getProperty('children')->__toString();

echo "== debug ==\n";
ob_start();
var_dump($f);
$dump = ob_get_clean();
foreach (explode("\n", $dump) as $line) {
    if (preg_match('/^  \["(.*)"\]=>$/', $line, $m)) {
        echo '  ', $m[1], "\n";
    }
}

echo "== refusals ==\n";
try {
    $r->children = 1;
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    unset($r->children);
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
var_dump(isset($r->children));
?>
--EXPECT--
Dom\HTMLCollection len=3 count=3 childNodes=6
  [0] a
  [1] b
  [2] c
item(0)=a item(9)=NULL named(x)=a dim[1]=b
live=4
doc=1:r empty=0
frag=1:q
same=true childNodes=false gebtn=false
== declared ==
Dom\Element: namespaceURI prefix localName tagName children
Dom\Document: children firstElementChild lastElementChild childElementCount implementation
Dom\DocumentFragment: children firstElementChild lastElementChild childElementCount
Property [ public Dom\HTMLCollection $children ]
== debug ==
  children
  firstElementChild
  lastElementChild
  childElementCount
  nodeType
  nodeName
  baseURI
  isConnected
  ownerDocument
  parentNode
  parentElement
  childNodes
  firstChild
  lastChild
  previousSibling
  nextSibling
  nodeValue
  textContent
== refusals ==
Error: Cannot modify readonly property Dom\Element::$children
Error: Cannot unset Dom\Element::$children
bool(true)
