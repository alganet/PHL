--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced tree refuses to canonicalize a node that is not in a document
--FILE--
<?php
// php 8.4's Dom\Node::C14N() asks whether the receiver is REACHABLE from its
// document, not whether a document made it: a fresh node, one lifted back out
// with removeChild, and a whole subtree under a DocumentFragment all carry a
// document pointer and none of them is in the document. The 2004 door asks
// nothing and canonicalizes every one of them to the empty string.
$dom_c14n_det_show = static function (string $label, callable $fn): void {
    try {
        printf("%-34s => %s\n", $label, var_export($fn(), true));
    } catch (Throwable $e) {
        printf("%-34s => %s(%d): %s\n", $label, $e::class, $e->getCode(), $e->getMessage());
    }
};
$dom_c14n_det_doc = static fn (): Dom\XMLDocument => Dom\XMLDocument::createEmpty();

$dom_c14n_det_d = $dom_c14n_det_doc();
$dom_c14n_det_r = $dom_c14n_det_d->appendChild($dom_c14n_det_d->createElement('r'));
$dom_c14n_det_show('document', fn () => $dom_c14n_det_d->C14N());
$dom_c14n_det_show('attached element', fn () => $dom_c14n_det_r->C14N());

// Every node kind a document can make, none of them linked in.
foreach ([
    'element'   => static fn (Dom\XMLDocument $d) => $d->createElement('o'),
    'text'      => static fn (Dom\XMLDocument $d) => $d->createTextNode('t'),
    'comment'   => static fn (Dom\XMLDocument $d) => $d->createComment('c'),
    'cdata'     => static fn (Dom\XMLDocument $d) => $d->createCDATASection('c'),
    'pi'        => static fn (Dom\XMLDocument $d) => $d->createProcessingInstruction('p', 'd'),
    'attribute' => static fn (Dom\XMLDocument $d) => $d->createAttribute('a'),
    'fragment'  => static fn (Dom\XMLDocument $d) => $d->createDocumentFragment(),
] as $dom_c14n_det_kind => $dom_c14n_det_make) {
    $dom_c14n_det_show("orphan $dom_c14n_det_kind", function () use ($dom_c14n_det_doc, $dom_c14n_det_make) {
        return $dom_c14n_det_make($dom_c14n_det_doc())->C14N();
    });
}

// A fragment is a root of its own, so nothing under it is in the document.
$dom_c14n_det_d2 = $dom_c14n_det_doc();
$dom_c14n_det_f = $dom_c14n_det_d2->createDocumentFragment();
$dom_c14n_det_fe = $dom_c14n_det_f->appendChild($dom_c14n_det_d2->createElement('fe'));
$dom_c14n_det_show('element under a fragment', fn () => $dom_c14n_det_fe->C14N());

// Deep inside an orphan subtree, and a node taken back out of the document.
$dom_c14n_det_d3 = $dom_c14n_det_doc();
$dom_c14n_det_o = $dom_c14n_det_d3->createElement('o');
$dom_c14n_det_oc = $dom_c14n_det_o->appendChild($dom_c14n_det_d3->createElement('oc'));
$dom_c14n_det_show('deep in an orphan subtree', fn () => $dom_c14n_det_oc->C14N());

$dom_c14n_det_d4 = $dom_c14n_det_doc();
$dom_c14n_det_r4 = $dom_c14n_det_d4->appendChild($dom_c14n_det_d4->createElement('r'));
$dom_c14n_det_c4 = $dom_c14n_det_r4->appendChild($dom_c14n_det_d4->createElement('c'));
$dom_c14n_det_r4->removeChild($dom_c14n_det_c4);
$dom_c14n_det_show('removed element', fn () => $dom_c14n_det_c4->C14N());

// An attribute and a text node that ARE in the document still answer.
$dom_c14n_det_d5 = $dom_c14n_det_doc();
$dom_c14n_det_r5 = $dom_c14n_det_d5->appendChild($dom_c14n_det_d5->createElement('r'));
$dom_c14n_det_r5->setAttribute('a', '1');
$dom_c14n_det_show('attached attribute', fn () => $dom_c14n_det_r5->getAttributeNode('a')->C14N());
$dom_c14n_det_t5 = $dom_c14n_det_r5->appendChild($dom_c14n_det_d5->createTextNode('x'));
$dom_c14n_det_show('attached text', fn () => $dom_c14n_det_t5->C14N());

// The refusal comes before every argument: a bad query and an inclusive-mode
// prefix list both reach it with no complaint of their own in front.
$dom_c14n_det_show('C14NFile on an orphan', function () use ($dom_c14n_det_doc) {
    return $dom_c14n_det_doc()->createElement('o')->C14NFile('/dev/null');
});
$dom_c14n_det_show('exclusive+comments on an orphan', function () use ($dom_c14n_det_doc) {
    return $dom_c14n_det_doc()->createElement('o')->C14N(true, true);
});
$dom_c14n_det_show('bad $xpath on an orphan', function () use ($dom_c14n_det_doc) {
    return $dom_c14n_det_doc()->createElement('o')->C14N(false, false, ['query' => '//']);
});
$dom_c14n_det_show('$nsPrefixes on an orphan', function () use ($dom_c14n_det_doc) {
    return $dom_c14n_det_doc()->createElement('o')->C14N(false, false, null, ['a']);
});

// The 2004 door is unchanged: it answers, it does not refuse.
$dom_c14n_det_show('2004 orphan element', fn () => (new DOMDocument())->createElement('o')->C14N());
$dom_c14n_det_show('2004 orphan attribute', fn () => (new DOMDocument())->createAttribute('a')->C14N());
--EXPECT--
document                           => '<r></r>'
attached element                   => '<r></r>'
orphan element                     => DOMException(3): Canonicalization can only happen on nodes attached to a document.
orphan text                        => DOMException(3): Canonicalization can only happen on nodes attached to a document.
orphan comment                     => DOMException(3): Canonicalization can only happen on nodes attached to a document.
orphan cdata                       => DOMException(3): Canonicalization can only happen on nodes attached to a document.
orphan pi                          => DOMException(3): Canonicalization can only happen on nodes attached to a document.
orphan attribute                   => DOMException(3): Canonicalization can only happen on nodes attached to a document.
orphan fragment                    => DOMException(3): Canonicalization can only happen on nodes attached to a document.
element under a fragment           => DOMException(3): Canonicalization can only happen on nodes attached to a document.
deep in an orphan subtree          => DOMException(3): Canonicalization can only happen on nodes attached to a document.
removed element                    => DOMException(3): Canonicalization can only happen on nodes attached to a document.
attached attribute                 => ' a="1"'
attached text                      => 'x'
C14NFile on an orphan              => DOMException(3): Canonicalization can only happen on nodes attached to a document.
exclusive+comments on an orphan    => DOMException(3): Canonicalization can only happen on nodes attached to a document.
bad $xpath on an orphan            => DOMException(3): Canonicalization can only happen on nodes attached to a document.
$nsPrefixes on an orphan           => DOMException(3): Canonicalization can only happen on nodes attached to a document.
2004 orphan element                => ''
2004 orphan attribute              => ''
