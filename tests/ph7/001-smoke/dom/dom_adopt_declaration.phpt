--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
adoptNode refuses to re-home a DTD declaration, and the three kinds answer differently
--FILE--
<?php
function dadec_try(string $lbl, callable $fn): void {
    try {
        $r = $fn();
        printf("%-32s ok %s\n", $lbl, is_object($r) ? get_class($r) : var_export($r, true));
    } catch (Throwable $e) {
        printf("%-32s %s(%d): %s\n", $lbl, get_class($e), $e->getCode(), $e->getMessage());
    }
}
$dadec_xml = '<!DOCTYPE r [<!ENTITY e "x"><!NOTATION n SYSTEM "s">]><r a="1">t</r>';
$dadec_mk = function () use ($dadec_xml) { $d = new DOMDocument; $d->loadXML($dadec_xml); return $d; };

// Every node type through the 2004 door. A doctype, a notation and a document
// are the Not Supported refusal; an entity declaration and a fragment are a
// bare false with no error at all; everything else moves.
$s = $dadec_mk();
$el = $s->documentElement;
$nodes = [
    'element'   => $el,
    'attr'      => $el->getAttributeNode('a'),
    'text'      => $el->firstChild,
    'entityref' => $s->createEntityReference('e'),
    'fragment'  => $s->createDocumentFragment(),
    'doctype'   => $s->doctype,
    'entity'    => $s->doctype->entities->item(0),
    'notation'  => $s->doctype->notations->item(0),
    'document'  => $s,
];
foreach ($nodes as $lbl => $n) {
    $dst = new DOMDocument;
    dadec_try(sprintf('%s(%d)', $lbl, $n->nodeType), fn() => $dst->adoptNode($n));
}

// The doctype reads the ARGUMENT's document for the strictness, exactly as the
// document does -- so a lax RECEIVER still throws and a lax argument does not.
$s = $dadec_mk();
$dst = new DOMDocument;
$dst->strictErrorChecking = false;
dadec_try('lax receiver, doctype', fn() => $dst->adoptNode($s->doctype));
$s = $dadec_mk();
$s->strictErrorChecking = false;
$dst = new DOMDocument;
set_error_handler(function ($no, $str) { printf("warning: %s\n", $str); return true; });
dadec_try('lax argument, doctype', fn() => $dst->adoptNode($s->doctype));
restore_error_handler();

// A NOTATION consults neither: it is the refusal in every mode. It is also the
// row that used to free the node twice, since a notation lives only in the
// DTD's own table and no child list holds it.
$s = $dadec_mk();
$dst = new DOMDocument;
$dst->strictErrorChecking = false;
dadec_try('lax receiver, notation', fn() => $dst->adoptNode($s->doctype->notations->item(0)));
$s = $dadec_mk();
$s->strictErrorChecking = false;
$dst = new DOMDocument;
dadec_try('lax argument, notation', fn() => $dst->adoptNode($s->doctype->notations->item(0)));

// php 8.4's tree keeps both refusals and has no false to answer, so the entity
// declaration it once passed over is adopted like any other node.
$m = Dom\XMLDocument::createFromString($dadec_xml);
$mdst = Dom\XMLDocument::createEmpty();
dadec_try('modern doctype', fn() => $mdst->adoptNode($m->doctype));
dadec_try('modern notation', fn() => $mdst->adoptNode($m->doctype->notations->item(0)));
dadec_try('modern entity', fn() => $mdst->adoptNode($m->doctype->entities->item(0)));

// And the source document is untouched by a refusal.
$s = $dadec_mk();
$dst = new DOMDocument;
try { $dst->adoptNode($s->doctype); } catch (Throwable $e) {}
try { $dst->adoptNode($s->doctype->notations->item(0)); } catch (Throwable $e) {}
printf("source keeps: doctype=%s notations=%d entities=%d\n",
    $s->doctype->name, $s->doctype->notations->length, $s->doctype->entities->length);
?>
--EXPECT--
element(1)                       ok DOMElement
attr(2)                          ok DOMAttr
text(3)                          ok DOMText
entityref(5)                     ok DOMEntityReference
fragment(11)                     ok false
doctype(10)                      DOMException(9): Not Supported Error
entity(17)                       ok false
notation(12)                     DOMException(9): Not Supported Error
document(9)                      DOMException(9): Not Supported Error
lax receiver, doctype            DOMException(9): Not Supported Error
warning: DOMDocument::adoptNode(): Not Supported Error
lax argument, doctype            ok false
lax receiver, notation           DOMException(9): Not Supported Error
lax argument, notation           DOMException(9): Not Supported Error
modern doctype                   DOMException(9): Not Supported Error
modern notation                  DOMException(9): Not Supported Error
modern entity                    ok Dom\Entity
source keeps: doctype=r notations=1 entities=1
