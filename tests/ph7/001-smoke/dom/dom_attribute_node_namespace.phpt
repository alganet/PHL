--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An attribute NODE moved between elements has its namespace resolved where it arrives
--FILE--
<?php
// An attribute NODE carries a namespace declaration of wherever it came FROM.
// Moving the node does not move the declaration, so the write has to resolve it
// against the element it arrives on -- or the bytes name a prefix nothing binds,
// or worse, one the target binds to a DIFFERENT URI.
$dom_anns = static function (string $xml, string $target, string $attr, bool $useNs) {
    $d = new DOMDocument;
    $d->loadXML($xml);
    $x = $d->getElementsByTagName('x')->item(0);
    $tgt = $target === 'r' ? $d->documentElement : $d->getElementsByTagName($target)->item(0);
    $an = null;
    foreach ($x->attributes as $a) if ($a->nodeName === $attr) $an = $a;
    $ret = $useNs ? $tgt->setAttributeNodeNS($an) : $tgt->setAttributeNode($an);
    $rr = new DOMDocument;
    $ok = @$rr->loadXML($d->saveXML());
    return $d->saveXML($d->documentElement)
        . ' | ' . $an->nodeName . ' ns=' . var_export($an->namespaceURI, true)
        . ' ret=' . ($ret === null ? 'null' : get_class($ret))
        . ' | reparse=' . ($ok ? 'ok' : 'FAILED')
        . ($ok ? ' round=' . $rr->saveXML($rr->documentElement) : '');
};

// Out of its declaration's scope: a declaration is made where it arrives.
$out = '<r><a xmlns:p="urn:p"><x p:k="1"/></a><b/></r>';
var_dump($dom_anns($out, 'b', 'p:k', true));
var_dump($dom_anns($out, 'b', 'p:k', false));

// ...and when the prefix is taken AT THE TARGET by another URI, php numbers it
// up rather than shadowing it -- shadowing would re-parse as a DIFFERENT
// namespace, which is a wrong answer that looks like well-formed XML.
var_dump($dom_anns('<r xmlns:p="urn:other"><a xmlns:p="urn:p"><x p:k="1"/></a><b/></r>',
    'b', 'p:k', true));

// Still IN scope where it arrives: the spelling is kept, even though a NEARER
// declaration binds the same URI under another prefix...
var_dump($dom_anns('<r xmlns:p="urn:p"><a xmlns:q="urn:p"><b/></a><x p:k="1"/></r>',
    'b', 'p:k', true));
// ...and even though the DEFAULT namespace binds it there.
var_dump($dom_anns('<r xmlns:p="urn:p"><a xmlns="urn:p"><b/></a><x p:k="1"/></r>',
    'b', 'p:k', true));
// Nothing nearer at all: also kept.
var_dump($dom_anns('<r xmlns:p="urn:p"><a><b/></a><x p:k="1"/></r>', 'b', 'p:k', true));

// A declaration LANDING on the element makes php judge its whole subtree from
// there: the sibling attribute's namespace, declared further down and still in
// scope where it stands, is re-declared on the element too and re-pointed at it.
$two = '<r><a xmlns:p="urn:p" xmlns:q="urn:q"><x p:k="1" q:m="2"/></a><b xmlns:p="urn:z"/></r>';
var_dump($dom_anns($two, 'r', 'p:k', true));
var_dump($dom_anns($two, 'r', 'q:m', true));
// ...while an arriving attribute that needed NO declaration leaves the subtree
// exactly as it was.
var_dump($dom_anns($two, 'a', 'p:k', true));

// A plain attribute (no namespace at all) is untouched by any of it.
var_dump($dom_anns('<r><a><x k="1"/></a><b/></r>', 'b', 'k', true));

// The target already declaring the same prefix and URI needs nothing new.
var_dump($dom_anns('<r><a xmlns:p="urn:p"><x p:k="1"/></a><b xmlns:p="urn:p"/></r>',
    'b', 'p:k', true));
?>
--EXPECT--
string(170) "<r><a xmlns:p="urn:p"><x/></a><b xmlns:p="urn:p" p:k="1"/></r> | p:k ns='urn:p' ret=null | reparse=ok round=<r><a xmlns:p="urn:p"><x/></a><b xmlns:p="urn:p" p:k="1"/></r>"
string(170) "<r><a xmlns:p="urn:p"><x/></a><b xmlns:p="urn:p" p:k="1"/></r> | p:k ns='urn:p' ret=null | reparse=ok round=<r><a xmlns:p="urn:p"><x/></a><b xmlns:p="urn:p" p:k="1"/></r>"
string(215) "<r xmlns:p="urn:other"><a xmlns:p="urn:p"><x/></a><b xmlns:p1="urn:p" p1:k="1"/></r> | p1:k ns='urn:p' ret=null | reparse=ok round=<r xmlns:p="urn:other"><a xmlns:p="urn:p"><x/></a><b xmlns:p1="urn:p" p1:k="1"/></r>"
string(170) "<r xmlns:p="urn:p"><a xmlns:q="urn:p"><b p:k="1"/></a><x/></r> | p:k ns='urn:p' ret=null | reparse=ok round=<r xmlns:p="urn:p"><a xmlns:q="urn:p"><b p:k="1"/></a><x/></r>"
string(166) "<r xmlns:p="urn:p"><a xmlns="urn:p"><b p:k="1"/></a><x/></r> | p:k ns='urn:p' ret=null | reparse=ok round=<r xmlns:p="urn:p"><a xmlns="urn:p"><b p:k="1"/></a><x/></r>"
string(138) "<r xmlns:p="urn:p"><a><b p:k="1"/></a><x/></r> | p:k ns='urn:p' ret=null | reparse=ok round=<r xmlns:p="urn:p"><a><b p:k="1"/></a><x/></r>"
string(282) "<r xmlns:p="urn:p" xmlns:q="urn:q" p:k="1"><a xmlns:p="urn:p" xmlns:q="urn:q"><x q:m="2"/></a><b xmlns:p="urn:z"/></r> | p:k ns='urn:p' ret=null | reparse=ok round=<r xmlns:p="urn:p" xmlns:q="urn:q" p:k="1"><a xmlns:p="urn:p" xmlns:q="urn:q"><x q:m="2"/></a><b xmlns:p="urn:z"/></r>"
string(282) "<r xmlns:q="urn:q" xmlns:p="urn:p" q:m="2"><a xmlns:p="urn:p" xmlns:q="urn:q"><x p:k="1"/></a><b xmlns:p="urn:z"/></r> | q:m ns='urn:q' ret=null | reparse=ok round=<r xmlns:q="urn:q" xmlns:p="urn:p" q:m="2"><a xmlns:p="urn:p" xmlns:q="urn:q"><x p:k="1"/></a><b xmlns:p="urn:z"/></r>"
string(218) "<r><a xmlns:p="urn:p" xmlns:q="urn:q" p:k="1"><x q:m="2"/></a><b xmlns:p="urn:z"/></r> | p:k ns='urn:p' ret=null | reparse=ok round=<r><a xmlns:p="urn:p" xmlns:q="urn:q" p:k="1"><x q:m="2"/></a><b xmlns:p="urn:z"/></r>"
string(97) "<r><a><x/></a><b k="1"/></r> | k ns=NULL ret=null | reparse=ok round=<r><a><x/></a><b k="1"/></r>"
string(170) "<r><a xmlns:p="urn:p"><x/></a><b xmlns:p="urn:p" p:k="1"/></r> | p:k ns='urn:p' ret=null | reparse=ok round=<r><a xmlns:p="urn:p"><x/></a><b xmlns:p="urn:p" p:k="1"/></r>"
