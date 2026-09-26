--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
registerNodeClass decides the class a document's nodes are wrapped in
--FILE--
<?php
class DomRncElement extends DOMElement
{
    public function tag(): string { return 'my:' . $this->tagName; }
}
class DomRncText extends DOMText {}
class DomRncPlain {}
abstract class DomRncAbstract extends DOMElement {}

// Registered BEFORE the document is read, which is the documented order: every
// element of this document -- parsed, navigated to, or made by a factory --
// comes back the program's own class, and its methods can be called on what a
// walk finds.
$d = new DOMDocument;
var_dump($d->registerNodeClass('DOMElement', 'DomRncElement'));
$d->loadXML('<r><k>t</k></r>');
var_dump(get_class($d->documentElement), $d->documentElement->tag(),
    get_class($d->documentElement->firstChild),
    get_class($d->createElement('x')),
    $d->documentElement === $d->documentElement);

// The lookup is by the class the extension would have USED and by nothing
// else, so a text node is untouched by an element registration -- and
// registering against a base nothing is wrapped as changes no wrapping at all.
var_dump(get_class($d->documentElement->firstChild->firstChild));
$n = new DOMDocument;
$n->registerNodeClass('DOMNode', 'DomRncElement');
$n->registerNodeClass('DOMCharacterData', 'DomRncText');
$n->loadXML('<r><k>t</k><!--c--></r>');
var_dump(get_class($n->documentElement),
    get_class($n->documentElement->firstChild->firstChild),
    get_class($n->documentElement->lastChild));

// It is the DOCUMENT's table: another document is untouched, and an imported
// node arrives wrapped in the receiver's class.
$other = new DOMDocument;
$other->loadXML('<r/>');
var_dump(get_class($other->documentElement),
    get_class($d->importNode($other->documentElement, true)));

// php's three refusals, each a different class of error: a name that is not a
// class at all, one that is not derived from the base, and an abstract one.
$cases = [
    ['DOMElement', 'DomRncPlain'],
    ['DOMElement', 'DomRncAbstract'],
    ['DOMElement', 'DomRncNoSuchClass'],
    ['DomRncPlain', 'DomRncElement'],
    ['DomRncNoSuchBase', 'DomRncElement'],
];
foreach ($cases as [$base, $ext]) {
    try {
        $d->registerNodeClass($base, $ext);
    } catch (Throwable $ex) {
        printf("%-17s %-17s %s: %s\n", $base, $ext, get_class($ex), $ex->getMessage());
    }
}
// ...and the two php accepts without blinking.
var_dump($d->registerNodeClass('DOMElement', 'DOMElement'),
    $d->registerNodeClass('DOMNode', 'DomRncElement'));

// A document COPY answers the same table -- php's copy does, and the two are
// separate afterwards.
$copy = $d->cloneNode(true);
var_dump(get_class($copy->documentElement), get_class((clone $d)->documentElement));

$m = new ReflectionMethod('DOMDocument', 'registerNodeClass');
$ps = [];
foreach ($m->getParameters() as $p) {
    $ps[] = ($p->getType() ? (string)$p->getType() . ' ' : '') . '$' . $p->getName();
}
printf("registerNodeClass(%s): %s\n", implode(', ', $ps),
    (string)($m->getTentativeReturnType() ?? $m->getReturnType() ?? '-'));
?>
--EXPECT--
bool(true)
string(13) "DomRncElement"
string(4) "my:r"
string(13) "DomRncElement"
string(13) "DomRncElement"
bool(true)
string(7) "DOMText"
string(10) "DOMElement"
string(7) "DOMText"
string(10) "DOMComment"
string(10) "DOMElement"
string(13) "DomRncElement"
DOMElement        DomRncPlain       Error: DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be a class name derived from DOMElement or null, DomRncPlain given
DOMElement        DomRncAbstract    ValueError: DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must not be an abstract class
DOMElement        DomRncNoSuchClass TypeError: DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be a valid class name or null, DomRncNoSuchClass given
DomRncPlain       DomRncElement     TypeError: DOMDocument::registerNodeClass(): Argument #1 ($baseClass) must be a class name derived from DOMNode, DomRncPlain given
DomRncNoSuchBase  DomRncElement     TypeError: DOMDocument::registerNodeClass(): Argument #1 ($baseClass) must be a class name derived from DOMNode, DomRncNoSuchBase given
bool(true)
bool(true)
string(10) "DOMElement"
string(10) "DOMElement"
registerNodeClass(string $baseClass, ?string $extendedClass): true
