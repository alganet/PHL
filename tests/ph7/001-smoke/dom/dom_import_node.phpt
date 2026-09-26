--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
importNode copies a node of another document, with php's shallow and namespace rules
--FILE--
<?php
$dom_imp_src = static function (): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML('<s xmlns:p="urn:p"><e a="1" p:b="2"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e></s>');
    return $d;
};
$dom_imp_dst = static function (string $xml = '<r/>'): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML($xml);
    return $d;
};
$dom_imp_show = static function (DOMDocument $d, $i): string {
    if (!($i instanceof DOMNode)) return var_export($i, true);
    return get_class($i) . ' ' . $d->saveXML($i)
        . ' | name=' . $i->nodeName . ' ns=' . var_export($i->namespaceURI, true)
        . ' kids=' . $i->childNodes->length
        . ' owner=' . var_export($i->ownerDocument === $d, true)
        . ' parent=' . ($i->parentNode ? $i->parentNode->nodeName : 'NULL')
        . ' connected=' . var_export($i->isConnected, true);
};

// Every node kind, shallow. Shallow does not mean bare: an element brings its
// attributes and its namespace declarations, only its children stay behind.
$s = $dom_imp_src();
$e = $s->documentElement->firstChild;
foreach ([['elem', $e], ['text', $e->firstChild->firstChild], ['t-elem', $e->firstChild],
          ['comment', $e->childNodes->item(1)], ['cdata', $e->childNodes->item(2)],
          ['pi', $e->childNodes->item(3)], ['attr', $e->getAttributeNode('a')],
          ['attr-ns', $e->getAttributeNodeNS('urn:p', 'b')]] as [$label, $n]) {
    $d = $dom_imp_dst();
    printf("%-8s %s\n", $label, $dom_imp_show($d, $d->importNode($n, false)));
}
// ...and deep, which is the only difference.
$d = $dom_imp_dst();
printf("deep     %s\n", $dom_imp_show($d, $d->importNode($e, true)));

// The source is untouched by either.
var_dump($s->saveXML($s->documentElement), $e->ownerDocument === $s);

// A node already OF this document is answered unchanged -- the same object, and
// not detached from where it is.
$d = $dom_imp_dst('<r><k/></r>');
$k = $d->documentElement->firstChild;
$r = $d->importNode($k, true);
var_dump($r === $k, $k->parentNode->nodeName, $d->saveXML($d->documentElement));
$q = $d->createElement('q');
var_dump($d->importNode($q, false) === $q);

// A DOCUMENT is a warning plus false, not an exception. (The warning is read
// through a handler rather than let out, so the expectation carries no path.)
$d = $dom_imp_dst();
set_error_handler(static function (int $no, string $msg): bool {
    printf("WARN(%d): %s\n", $no, $msg);
    return true;
});
var_dump($d->importNode($s, true));
restore_error_handler();

// Two imports of one node are two copies, and each is an orphan until linked.
$d = $dom_imp_dst();
$i1 = $d->importNode($e, true);
$i2 = $d->importNode($e, true);
var_dump($i1 === $i2, $i1->isConnected, $i2->isConnected);
$d->documentElement->appendChild($i1);
$d->documentElement->appendChild($i2);
var_dump($d->saveXML($d->documentElement));

// A FRAGMENT brings its children only when deep.
$f = $s->createDocumentFragment();
$f->appendChild($s->createElement('x'));
$f->appendChild($s->createElement('y'));
$d = $dom_imp_dst();
printf("frag deep=%d shallow=%d\n", $d->importNode($f, true)->childNodes->length,
    $d->importNode($f, false)->childNodes->length);

// A namespaced ATTRIBUTE is the one kind libxml leaves unfinished: php re-points
// the copy at a PREFIXED binding on the TARGET's root -- reusing one the root
// already has, which can change the prefix, and declaring it there otherwise.
foreach (['<r/>', '<r xmlns:p="urn:p"/>', '<r xmlns:z="urn:p"/>', '<r xmlns="urn:p"/>',
          '<r xmlns:p="urn:other"/>'] as $host) {
    $s2 = $dom_imp_src();
    $an = $s2->documentElement->firstChild->getAttributeNodeNS('urn:p', 'b');
    $d = $dom_imp_dst($host);
    $i = $d->importNode($an, false);
    printf("%-24s %s %s | doc=%s\n", $host, $i->nodeName, var_export($i->namespaceURI, true),
        $d->saveXML($d->documentElement));
    $d->documentElement->setAttributeNodeNS($i);
    printf("%-24s set: %s\n", '', $d->saveXML($d->documentElement));
}

// An element whose namespace is declared on an ancestor it does not bring along
// takes a declaration of its own.
$s3 = new DOMDocument;
$s3->loadXML('<s xmlns:p="urn:p" xmlns="urn:d"><p:e><k/></p:e></s>');
$d = $dom_imp_dst();
$i = $d->importNode($s3->documentElement->firstChild, true);
var_dump($d->saveXML($i));
$d->documentElement->appendChild($i);
var_dump($d->saveXML($d->documentElement));

// Reflection sees php's signature.
$rm = new ReflectionMethod('DOMDocument', 'importNode');
$ps = [];
foreach ($rm->getParameters() as $p) {
    $ps[] = ($p->getType() ? (string)$p->getType() . ' ' : '') . '$' . $p->getName()
        . ($p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
}
var_dump(implode(', ', $ps), $rm->getReturnType(), $rm->getTentativeReturnType());
try { $d->importNode(1, false); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
elem     DOMElement <e xmlns:p="urn:p" a="1" p:b="2"/> | name=e ns=NULL kids=0 owner=true parent=NULL connected=false
text     DOMText tx | name=#text ns=NULL kids=0 owner=true parent=NULL connected=false
t-elem   DOMElement <t/> | name=t ns=NULL kids=0 owner=true parent=NULL connected=false
comment  DOMComment <!--c--> | name=#comment ns=NULL kids=0 owner=true parent=NULL connected=false
cdata    DOMCdataSection <![CDATA[cd]]> | name=#cdata-section ns=NULL kids=0 owner=true parent=NULL connected=false
pi       DOMProcessingInstruction <?pi d?> | name=pi ns=NULL kids=0 owner=true parent=NULL connected=false
attr     DOMAttr  a="1" | name=a ns=NULL kids=1 owner=true parent=NULL connected=false
attr-ns  DOMAttr  p:b="2" | name=p:b ns='urn:p' kids=1 owner=true parent=NULL connected=false
deep     DOMElement <e xmlns:p="urn:p" a="1" p:b="2"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e> | name=e ns=NULL kids=4 owner=true parent=NULL connected=false
string(83) "<s xmlns:p="urn:p"><e a="1" p:b="2"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e></s>"
bool(true)
bool(true)
string(1) "r"
string(11) "<r><k/></r>"
bool(true)
WARN(2): DOMDocument::importNode(): Cannot import: Node Type Not Supported
bool(false)
bool(false)
bool(false)
bool(false)
string(159) "<r><e xmlns:p="urn:p" a="1" p:b="2"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e><e xmlns:p="urn:p" a="1" p:b="2"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e></r>"
frag deep=2 shallow=0
<r/>                     p:b 'urn:p' | doc=<r xmlns:p="urn:p"/>
                         set: <r xmlns:p="urn:p" p:b="2"/>
<r xmlns:p="urn:p"/>     p:b 'urn:p' | doc=<r xmlns:p="urn:p"/>
                         set: <r xmlns:p="urn:p" p:b="2"/>
<r xmlns:z="urn:p"/>     z:b 'urn:p' | doc=<r xmlns:z="urn:p"/>
                         set: <r xmlns:z="urn:p" z:b="2"/>
<r xmlns="urn:p"/>       p:b 'urn:p' | doc=<r xmlns="urn:p" xmlns:p="urn:p"/>
                         set: <r xmlns="urn:p" xmlns:p="urn:p" p:b="2"/>
<r xmlns:p="urn:other"/> default:b 'urn:p' | doc=<r xmlns:p="urn:other" xmlns:default="urn:p"/>
                         set: <r xmlns:p="urn:other" xmlns:default="urn:p" default:b="2"/>
string(45) "<p:e xmlns:p="urn:p" xmlns="urn:d"><k/></p:e>"
string(52) "<r><p:e xmlns:p="urn:p" xmlns="urn:d"><k/></p:e></r>"
string(33) "DOMNode $node, bool $deep = false"
NULL
NULL
TypeError: DOMDocument::importNode(): Argument #1 ($node) must be of type DOMNode, int given
