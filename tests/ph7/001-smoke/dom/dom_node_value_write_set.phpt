--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A nodeValue/textContent write php ignores leaves the node alone
--FILE--
<?php
$dom_vws_doc = static function (): DOMDocument {
    $d = new DOMDocument;
    $d->substituteEntities = false;
    $d->loadXML('<!DOCTYPE r [<!ENTITY a "AA">]>'
        . '<r><e>t<i/></e><!--c--><?pi p?><![CDATA[cd]]>&a;</r>');
    return $d;
};
$dom_vws_pick = static function (DOMDocument $d, string $kind): DOMNode {
    switch ($kind) {
        case 'element':   return $d->getElementsByTagName('e')->item(0);
        case 'text':      return $d->getElementsByTagName('e')->item(0)->firstChild;
        case 'comment':   return $d->documentElement->childNodes->item(1);
        case 'pi':        return $d->documentElement->childNodes->item(2);
        case 'cdata':     return $d->documentElement->childNodes->item(3);
        case 'entityref': return $d->documentElement->childNodes->item(4);
        case 'attr':
            $d->documentElement->setAttribute('k', 'v');
            return $d->documentElement->getAttributeNode('k');
        case 'fragment':
            $f = $d->createDocumentFragment();
            $f->appendChild($d->createElement('fc'));
            return $f;
        default:          return $d;
    }
};

// The two writers do NOT take the same node kinds, and everything off the list
// is a silent no-op -- the write is accepted and the node is untouched. An
// ENTITY REFERENCE is the one refusal, and only from textContent.
$kinds = ['element', 'text', 'comment', 'pi', 'cdata', 'entityref', 'attr',
          'fragment', 'document'];
foreach (['nodeValue', 'textContent'] as $prop) {
    foreach ($kinds as $kind) {
        $d = $dom_vws_doc();
        $n = $dom_vws_pick($d, $kind);
        try {
            $n->$prop = 'NEW';
            $said = 'ok';
        } catch (Throwable $ex) {
            $said = get_class($ex) . ': ' . $ex->getMessage();
        }
        printf("%-11s %-9s %-64s %s\n", $prop, $kind, $said,
            str_replace("\n", '', $d->saveXML($kind === 'document' ? null : $n)));
    }
}

// The doctype keeps its internal subset -- the write used to empty it -- and an
// entity reference keeps the declaration's children, which belong to the DTD.
$d = $dom_vws_doc();
$dt = $d->firstChild;
$dt->nodeValue = 'x';
$dt->textContent = 'y';
$ref = $d->documentElement->lastChild;
$ref->nodeValue = 'z';
var_dump($dt->childNodes->length, $ref->firstChild->nodeName,
    str_replace("\n", '', $d->saveXML()));

// The refusal answers BEFORE the type check on the one property that refuses,
// and after it everywhere else.
$d = $dom_vws_doc();
$ref = $d->documentElement->lastChild;
foreach (['textContent', 'nodeValue'] as $prop) {
    try {
        $ref->$prop = [];
    } catch (Throwable $ex) {
        printf("%s: %s: %s\n", $prop, get_class($ex), $ex->getMessage());
    }
}
?>
--EXPECT--
nodeValue   element   ok                                                               <e>NEW</e>
nodeValue   text      ok                                                               NEW
nodeValue   comment   ok                                                               <!--NEW-->
nodeValue   pi        ok                                                               <?pi NEW?>
nodeValue   cdata     ok                                                               <![CDATA[NEW]]>
nodeValue   entityref ok                                                               &a;
nodeValue   attr      ok                                                                k="NEW"
nodeValue   fragment  ok                                                               <fc/>
nodeValue   document  ok                                                               <?xml version="1.0"?><!DOCTYPE r [<!ENTITY a "AA">]><r><e>t<i/></e><!--c--><?pi p?><![CDATA[cd]]>&a;</r>
textContent element   ok                                                               <e>NEW</e>
textContent text      ok                                                               NEW
textContent comment   ok                                                               <!--NEW-->
textContent pi        ok                                                               <?pi NEW?>
textContent cdata     ok                                                               <![CDATA[NEW]]>
textContent entityref Error: Cannot modify readonly property DOMEntityReference::$textContent &a;
textContent attr      ok                                                                k="NEW"
textContent fragment  ok                                                               NEW
textContent document  ok                                                               <?xml version="1.0"?><!DOCTYPE r [<!ENTITY a "AA">]><r><e>t<i/></e><!--c--><?pi p?><![CDATA[cd]]>&a;</r>
int(1)
string(1) "a"
string(104) "<?xml version="1.0"?><!DOCTYPE r [<!ENTITY a "AA">]><r><e>t<i/></e><!--c--><?pi p?><![CDATA[cd]]>&a;</r>"
textContent: Error: Cannot modify readonly property DOMEntityReference::$textContent
nodeValue: TypeError: Cannot assign array to property DOMNode::$nodeValue of type ?string
