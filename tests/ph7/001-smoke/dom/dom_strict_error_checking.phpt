--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMDocument::$strictErrorChecking turns a DOM refusal into a warning
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "diag $no: ", str_replace("\n", '', $str), "\n"; return true; });
$mk = function ($strict) {
    $d = new DOMDocument;
    $d->loadXML('<r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>');
    $d->strictErrorChecking = $strict;
    return $d;
};
$other = function () { $o = new DOMDocument; $o->loadXML('<q><z/></q>'); return $o; };
$sh = function ($v) { return is_object($v) ? get_class($v) : var_export($v, true); };
$cases = [
    // the tree four: a warning and false where php would have thrown
    'appendChild wrong doc' => fn($d, $o) => $d->documentElement->appendChild($o->documentElement),
    'appendChild self'      => fn($d, $o) => $d->documentElement->appendChild($d->documentElement),
    'insertBefore notfound' => fn($d, $o) => $d->documentElement->insertBefore($d->createElement('x'), $o->documentElement),
    'replaceChild notfound' => fn($d, $o) => $d->documentElement->replaceChild($d->createElement('x'), $o->documentElement),
    'removeChild notfound'  => fn($d, $o) => $d->documentElement->removeChild($o->documentElement),
    // the create family
    'createElement'         => fn($d, $o) => $d->createElement('1bad'),
    'createElementNS'       => fn($d, $o) => $d->createElementNS(null, 'p:x'),
    'createAttribute'       => fn($d, $o) => $d->createAttribute('1bad'),
    'createAttributeNS'     => fn($d, $o) => $d->createAttributeNS('', 'p:x'),
    'createPI'              => fn($d, $o) => $d->createProcessingInstruction('1bad'),
    'createEntityReference' => fn($d, $o) => $d->createEntityReference('1bad'),
    // the attribute family
    'removeAttributeNode'   => fn($d, $o) => $d->documentElement->removeAttributeNode($o->createAttribute('zz')),
    'setAttributeNode'      => fn($d, $o) => $d->documentElement->setAttributeNode($o->createAttribute('zz')),
    // php declares these void: the non-strict answer is NOTHING, not false
    'setAttributeNS'        => fn($d, $o) => $d->documentElement->setAttributeNS(null, 'q:x', 'v'),
    'setIdAttribute'        => fn($d, $o) => $d->documentElement->setIdAttribute('nope', true),
    'setIdAttributeNS'      => fn($d, $o) => $d->documentElement->setIdAttributeNS('urn:p', 'nope', true),
    'setIdAttributeNode'    => fn($d, $o) => $d->documentElement->setIdAttributeNode($o->createAttribute('zz'), true),
    // character data
    'substringData'         => fn($d, $o) => $d->documentElement->lastChild->substringData(999, 1),
    'deleteData'            => fn($d, $o) => $d->documentElement->lastChild->deleteData(999, 2),
    // ...and the three php refuses STRICTLY whatever the flag says
    'setAttribute'          => fn($d, $o) => $d->documentElement->setAttribute('1bad', 'v'),
    'toggleAttribute'       => fn($d, $o) => $d->documentElement->toggleAttribute('1bad'),
];
foreach ($cases as $label => $fn) {
    foreach ([true, false] as $strict) {
        $d = $mk($strict);
        printf("%-22s strict=%d\n", $label, (int)$strict);
        try { echo '  -> ', $sh($fn($d, $other())), "\n"; }
        catch (\Throwable $e) { echo '  ', get_class($e), '(', $e->getCode(), '): ', $e->getMessage(), "\n"; }
        // a refused operation leaves the tree exactly as it was, either way
        echo '  ', str_replace("\n", '', $d->saveXML()), "\n";
    }
}
// A property WRITE has no accessor name to print: php attributes the warning to
// the CALLER's scope instead.
echo "== prefix write\n";
$d = $mk(false);
$d->documentElement->firstChild->prefix = 'xml';
// (a NAMED function, not a closure: php prints a closure's name with the file
// path in it, which no expectation can pin)
function phl_dom_strict_prefix_writer($d) { $d->documentElement->firstChild->prefix = 'xml'; }
phl_dom_strict_prefix_writer($mk(false));
try { $mk(true)->documentElement->firstChild->prefix = 'xml'; }
catch (\Throwable $e) { echo '  ', get_class($e), '(', $e->getCode(), '): ', $e->getMessage(), "\n"; }

// adoptNode is the one refusal that reads the ARGUMENT's document, not the
// receiver's.
echo "== adoptNode reads the argument's flag\n";
foreach ([[true, true], [true, false], [false, true], [false, false]] as [$a, $b]) {
    $d = $mk($a); $o = $other(); $o->strictErrorChecking = $b;
    printf("  recv=%d arg=%d ", (int)$a, (int)$b);
    try { echo '-> ', $sh($d->adoptNode($o)), "\n"; }
    catch (\Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

// The flag is document state: it survives a reload onto the same object and a
// clone carries it.
$d = $mk(false);
$d->loadXML('<z/>');
$c = $d->cloneNode(true);
var_dump($d->strictErrorChecking, $c->strictErrorChecking, (new DOMDocument)->strictErrorChecking);
--EXPECT--
appendChild wrong doc  strict=1
  ->   DOMException(4): Wrong Document Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
appendChild wrong doc  strict=0
  -> diag 2: DOMNode::appendChild(): Wrong Document Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
appendChild self       strict=1
  ->   DOMException(3): Hierarchy Request Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
appendChild self       strict=0
  -> diag 2: DOMNode::appendChild(): Hierarchy Request Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
insertBefore notfound  strict=1
  ->   DOMException(8): Not Found Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
insertBefore notfound  strict=0
  -> diag 2: DOMNode::insertBefore(): Not Found Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
replaceChild notfound  strict=1
  ->   DOMException(8): Not Found Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
replaceChild notfound  strict=0
  -> diag 2: DOMNode::replaceChild(): Not Found Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
removeChild notfound   strict=1
  ->   DOMException(8): Not Found Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
removeChild notfound   strict=0
  -> diag 2: DOMNode::removeChild(): Not Found Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createElement          strict=1
  ->   DOMException(5): Invalid Character Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createElement          strict=0
  -> diag 2: DOMDocument::createElement(): Invalid Character Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createElementNS        strict=1
  ->   DOMException(14): Namespace Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createElementNS        strict=0
  -> diag 2: DOMDocument::createElementNS(): Namespace Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createAttribute        strict=1
  ->   DOMException(5): Invalid Character Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createAttribute        strict=0
  -> diag 2: DOMDocument::createAttribute(): Invalid Character Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createAttributeNS      strict=1
  ->   DOMException(14): Namespace Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createAttributeNS      strict=0
  -> diag 2: DOMDocument::createAttributeNS(): Namespace Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createPI               strict=1
  ->   DOMException(5): Invalid Character Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createPI               strict=0
  -> diag 2: DOMDocument::createProcessingInstruction(): Invalid Character Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createEntityReference  strict=1
  ->   DOMException(5): Invalid Character Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
createEntityReference  strict=0
  -> diag 2: DOMDocument::createEntityReference(): Invalid Character Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
removeAttributeNode    strict=1
  ->   DOMException(8): Not Found Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
removeAttributeNode    strict=0
  -> diag 2: DOMElement::removeAttributeNode(): Not Found Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setAttributeNode       strict=1
  ->   DOMException(4): Wrong Document Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setAttributeNode       strict=0
  -> diag 2: DOMElement::setAttributeNode(): Wrong Document Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setAttributeNS         strict=1
  ->   DOMException(14): Namespace Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setAttributeNS         strict=0
  -> diag 2: DOMElement::setAttributeNS(): Namespace Error
NULL
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setIdAttribute         strict=1
  ->   DOMException(8): Not Found Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setIdAttribute         strict=0
  -> diag 2: DOMElement::setIdAttribute(): Not Found Error
NULL
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setIdAttributeNS       strict=1
  ->   DOMException(8): Not Found Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setIdAttributeNS       strict=0
  -> diag 2: DOMElement::setIdAttributeNS(): Not Found Error
NULL
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setIdAttributeNode     strict=1
  ->   DOMException(8): Not Found Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setIdAttributeNode     strict=0
  -> diag 2: DOMElement::setIdAttributeNode(): Not Found Error
NULL
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
substringData          strict=1
  ->   DOMException(1): Index Size Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
substringData          strict=0
  -> diag 2: DOMCharacterData::substringData(): Index Size Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
deleteData             strict=1
  ->   DOMException(1): Index Size Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
deleteData             strict=0
  -> diag 2: DOMCharacterData::deleteData(): Index Size Error
false
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setAttribute           strict=1
  ->   DOMException(5): Invalid Character Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
setAttribute           strict=0
  ->   DOMException(5): Invalid Character Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
toggleAttribute        strict=1
  ->   DOMException(5): Invalid Character Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
toggleAttribute        strict=0
  ->   DOMException(5): Invalid Character Error
  <?xml version="1.0"?><r xmlns:p="urn:p"><p:a k="v"/><b/>text</r>
== prefix write
diag 2: main(): Namespace Error
diag 2: phl_dom_strict_prefix_writer(): Namespace Error
  DOMException(14): Namespace Error
== adoptNode reads the argument's flag
  recv=1 arg=1 -> DOMException: Not Supported Error
  recv=1 arg=0 -> diag 2: DOMDocument::adoptNode(): Not Supported Error
false
  recv=0 arg=1 -> DOMException: Not Supported Error
  recv=0 arg=0 -> diag 2: DOMDocument::adoptNode(): Not Supported Error
false
bool(false)
bool(false)
bool(true)
