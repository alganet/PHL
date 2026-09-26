--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
adoptNode moves a node between documents, wrappers and all
--FILE--
<?php
$dom_ad_src = static function (): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML('<s xmlns:p="urn:p"><e a="1" p:b="2"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e></s>');
    return $d;
};
$dom_ad_dst = static function (string $xml = '<r/>'): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML($xml);
    return $d;
};

// The answer is the SAME object, moved: the source loses the node, this document
// owns it, and it arrives DETACHED.
$s = $dom_ad_src();
$d = $dom_ad_dst();
$e = $s->documentElement->firstChild;
$kid = $e->firstChild;
$r = $d->adoptNode($e);
var_dump($r === $e, $r->ownerDocument === $d, $r->parentNode, $r->isConnected);
var_dump($s->saveXML($s->documentElement), $d->saveXML($d->documentElement));
// ...and every node UNDER it changed document too, wrappers PHP already holds
// included.
var_dump($kid->ownerDocument === $d, $kid->nodeName, $kid->textContent);
$d->documentElement->appendChild($r);
var_dump($d->saveXML($d->documentElement));

// Adopting a node of THIS document still unlinks it -- which empties a document
// when the node is its root.
$d = $dom_ad_dst();
$root = $d->documentElement;
var_dump($d->adoptNode($root) === $root, $root->parentNode, $d->saveXML());

// A DOCUMENT is the Not Supported DOMException; a FRAGMENT is a plain false.
$s = $dom_ad_src();
$d = $dom_ad_dst();
try { $d->adoptNode($s); } catch (Throwable $ex) {
    printf("document: %s(%d): %s\n", get_class($ex), $ex->getCode(), $ex->getMessage());
}
$f = $s->createDocumentFragment();
$f->appendChild($s->createElement('x'));
var_dump($d->adoptNode($f));

// Every other node kind moves. An attribute comes off its element.
foreach (['elem', 'text', 'comment', 'cdata', 'pi', 'attr', 'attr-ns', 'fresh'] as $label) {
    $s = $dom_ad_src();
    $d = $dom_ad_dst();
    $e = $s->documentElement->firstChild;
    $n = match ($label) {
        'elem' => $e,
        'text' => $e->firstChild->firstChild,
        'comment' => $e->childNodes->item(1),
        'cdata' => $e->childNodes->item(2),
        'pi' => $e->childNodes->item(3),
        'attr' => $e->getAttributeNode('a'),
        'attr-ns' => $e->getAttributeNodeNS('urn:p', 'b'),
        'fresh' => $s->createElement('q'),
    };
    $r = $d->adoptNode($n);
    printf("%-8s %s owner=%s ns=%s | src=%s\n", $label, get_class($r),
        var_export($r->ownerDocument === $d, true), var_export($r->namespaceURI, true),
        $s->saveXML($s->documentElement));
}

// NOTHING is re-declared by the adoption itself: the node keeps answering its
// namespace while carrying no declaration of it, and the declaration appears
// when it is LINKED.
$s = new DOMDocument;
$s->loadXML('<s xmlns:p="urn:p"><p:e><k/></p:e></s>');
$d = $dom_ad_dst();
$pe = $s->documentElement->firstChild;
$d->adoptNode($pe);
var_dump($d->saveXML($pe), $pe->namespaceURI, $pe->prefix);
$d->documentElement->appendChild($pe);
var_dump($d->saveXML($d->documentElement));

// An adopted node cannot go back into the document it left.
$s = $dom_ad_src();
$d = $dom_ad_dst();
$q = $s->createElement('q');
$d->adoptNode($q);
try { $s->documentElement->appendChild($q); }
catch (Throwable $ex) { printf("back: %s(%d): %s\n", get_class($ex), $ex->getCode(), $ex->getMessage()); }

// A namespaced attribute adopted and then written lands in a binding of the
// TARGET's making.
foreach (['<r/>', '<r xmlns:p="urn:p"/>', '<r xmlns:z="urn:p"/>', '<r xmlns="urn:p"/>',
          '<r xmlns:p="urn:other"/>'] as $host) {
    $s = $dom_ad_src();
    $d = $dom_ad_dst($host);
    $an = $s->documentElement->firstChild->getAttributeNodeNS('urn:p', 'b');
    $a = $d->adoptNode($an);
    $d->documentElement->setAttributeNodeNS($a);
    printf("%-24s %s | %s\n", $host, $a->nodeName, $d->saveXML($d->documentElement));
}

// Reflection sees php's signature.
$rm = new ReflectionMethod('DOMDocument', 'adoptNode');
$ps = [];
foreach ($rm->getParameters() as $p) {
    $ps[] = ($p->getType() ? (string)$p->getType() . ' ' : '') . '$' . $p->getName();
}
var_dump(implode(', ', $ps), $rm->getReturnType(),
    $rm->getTentativeReturnType() ? (string)$rm->getTentativeReturnType() : null);

// Adoption takes an ID attribute OUT of the ID table and stops it being an ID at
// all -- in neither document is the element findable by it afterwards, whether
// the attribute or its whole element was the thing adopted.
$s = new DOMDocument;
$s->loadXML('<s><e id="a1"/></s>');
$e = $s->documentElement->firstChild;
$e->setIdAttribute('id', true);
var_dump($s->getElementById('a1')?->nodeName);
$d = $dom_ad_dst();
$an = $e->getAttributeNode('id');
$d->adoptNode($an);
var_dump($s->getElementById('a1')?->nodeName, $d->getElementById('a1')?->nodeName,
    $an->isId(), $s->saveXML($s->documentElement));
$s2 = new DOMDocument;
$s2->loadXML('<s><e id="b1"/></s>');
$e2 = $s2->documentElement->firstChild;
$e2->setIdAttribute('id', true);
$d2 = $dom_ad_dst();
$d2->adoptNode($e2);
$d2->documentElement->appendChild($e2);
var_dump($s2->getElementById('b1')?->nodeName, $d2->getElementById('b1')?->nodeName,
    $e2->getAttributeNode('id')->isId());
?>
--EXPECT--
bool(true)
bool(true)
NULL
bool(false)
string(20) "<s xmlns:p="urn:p"/>"
string(4) "<r/>"
bool(true)
string(1) "t"
string(2) "tx"
string(83) "<r><e xmlns:p="urn:p" a="1" p:b="2"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e></r>"
bool(true)
NULL
string(22) "<?xml version="1.0"?>
"
document: DOMException(9): Not Supported Error
bool(false)
elem     DOMElement owner=true ns=NULL | src=<s xmlns:p="urn:p"/>
text     DOMText owner=true ns=NULL | src=<s xmlns:p="urn:p"><e a="1" p:b="2"><t/><!--c--><![CDATA[cd]]><?pi d?></e></s>
comment  DOMComment owner=true ns=NULL | src=<s xmlns:p="urn:p"><e a="1" p:b="2"><t>tx</t><![CDATA[cd]]><?pi d?></e></s>
cdata    DOMCdataSection owner=true ns=NULL | src=<s xmlns:p="urn:p"><e a="1" p:b="2"><t>tx</t><!--c--><?pi d?></e></s>
pi       DOMProcessingInstruction owner=true ns=NULL | src=<s xmlns:p="urn:p"><e a="1" p:b="2"><t>tx</t><!--c--><![CDATA[cd]]></e></s>
attr     DOMAttr owner=true ns=NULL | src=<s xmlns:p="urn:p"><e p:b="2"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e></s>
attr-ns  DOMAttr owner=true ns='urn:p' | src=<s xmlns:p="urn:p"><e a="1"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e></s>
fresh    DOMElement owner=true ns=NULL | src=<s xmlns:p="urn:p"><e a="1" p:b="2"><t>tx</t><!--c--><![CDATA[cd]]><?pi d?></e></s>
string(15) "<p:e><k/></p:e>"
string(5) "urn:p"
string(1) "p"
string(38) "<r><p:e xmlns:p="urn:p"><k/></p:e></r>"
back: DOMException(4): Wrong Document Error
<r/>                     p:b | <r xmlns:p="urn:p" p:b="2"/>
<r xmlns:p="urn:p"/>     p:b | <r xmlns:p="urn:p" p:b="2"/>
<r xmlns:z="urn:p"/>     z:b | <r xmlns:z="urn:p" z:b="2"/>
<r xmlns="urn:p"/>       b | <r xmlns="urn:p" b="2"/>
<r xmlns:p="urn:other"/> p1:b | <r xmlns:p="urn:other" xmlns:p1="urn:p" p1:b="2"/>
string(13) "DOMNode $node"
NULL
string(13) "DOMNode|false"
string(1) "e"
NULL
NULL
bool(false)
string(11) "<s><e/></s>"
NULL
NULL
bool(false)
