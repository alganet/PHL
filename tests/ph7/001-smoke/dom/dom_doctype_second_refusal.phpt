--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
appendChild is the only door that refuses a second DOCTYPE, and the two trees ask different questions
--FILE--
<?php
function dtsr_try(string $lbl, callable $fn): void {
    try {
        $r = $fn();
        printf("%-38s ok %s\n", $lbl, is_object($r) ? get_class($r) : var_export($r, true));
    } catch (Throwable $e) {
        printf("%-38s %s(%d): %s\n", $lbl, get_class($e), $e->getCode(), $e->getMessage());
    }
}
$dtsr_imp = new DOMImplementation;
$dtsr_mk = fn(string $n) => (new DOMImplementation)->createDocumentType($n);

// Only appendChild refuses. insertBefore and replaceChild take a second
// doctype without a word -- on both trees.
$d = $dtsr_imp->createDocument(null, 'r', $dtsr_mk('a'));
dtsr_try('append 2nd', fn() => $d->appendChild($dtsr_mk('b')));
$d = $dtsr_imp->createDocument(null, 'r', $dtsr_mk('a'));
dtsr_try('insertBefore 2nd', fn() => $d->insertBefore($dtsr_mk('b'), $d->documentElement));
$d = $dtsr_imp->createDocument(null, 'r', $dtsr_mk('a'));
dtsr_try('replaceChild over the doctype', fn() => $d->replaceChild($dtsr_mk('b'), $d->doctype));

// The 2004 door asks about the document's INTERNAL SUBSET, not its children,
// which shows on three faces: an ELEMENT receiver refuses too; a first doctype
// installed by insertBefore never wrote the slot, so a second is accepted; and
// re-appending the document's OWN subset is not a second one.
$d = $dtsr_imp->createDocument(null, 'r', $dtsr_mk('a'));
dtsr_try('element receiver', fn() => $d->documentElement->appendChild($dtsr_mk('b')));
$d = new DOMDocument;
$d->appendChild($d->createElement('r'));
$d->insertBefore($dtsr_mk('a'), $d->documentElement);
dtsr_try('2nd when 1st is child-only', fn() => $d->appendChild($dtsr_mk('b')));
$d = $dtsr_imp->createDocument(null, 'r', $dtsr_mk('a'));
dtsr_try('re-append the same doctype', fn() => $d->appendChild($d->doctype));
$d = new DOMDocument;
$d->appendChild($d->createElement('r'));
dtsr_try('after an element, no subset yet', fn() => $d->appendChild($dtsr_mk('a')));

// The wrong-document screen answers before it.
$d = $dtsr_imp->createDocument(null, 'r', $dtsr_mk('a'));
$dtsr_other = $dtsr_imp->createDocument(null, 'q', $dtsr_mk('z'));
dtsr_try('foreign 2nd doctype', fn() => $d->appendChild($dtsr_other->doctype));

// The refusal runs AFTER the adoption: the argument comes out owned and
// parentless, and nothing was linked.
$d = $dtsr_imp->createDocument(null, 'r', $dtsr_mk('a'));
$dtsr_dt = $dtsr_mk('b');
printf("before: owner=%s\n", $dtsr_dt->ownerDocument === null ? 'null' : 'set');
try { $d->appendChild($dtsr_dt); } catch (Throwable $e) {}
printf("after : owner=%s parent=%s children=%d\n",
    $dtsr_dt->ownerDocument === null ? 'null' : 'set',
    $dtsr_dt->parentNode ? 'set' : 'null', $d->childNodes->length);

// strictErrorChecking governs it: a warning and false, not an exception.
// Taken through a handler so the sentence is read without its file and line.
$d = $dtsr_imp->createDocument(null, 'r', $dtsr_mk('a'));
$d->strictErrorChecking = false;
set_error_handler(function ($no, $str) { printf("warning: %s\n", $str); return true; });
dtsr_try('lax receiver', fn() => $d->appendChild($dtsr_mk('b')));
restore_error_handler();

// php 8.4's tree asks the WHATWG question over the CHILDREN instead, states
// its own sentence for a receiver that is no document, and so counts the very
// doctype the older door misses.
$dtsr_src = Dom\XMLDocument::createFromString('<!DOCTYPE a><q/>');
$m = Dom\XMLDocument::createFromString('<!DOCTYPE a><r/>');
dtsr_try('modern 2nd', fn() => $m->appendChild($m->importNode($dtsr_src->doctype)));
dtsr_try('modern element receiver',
    fn() => $m->documentElement->appendChild($m->importNode($dtsr_src->doctype)));
$m = Dom\XMLDocument::createFromString('<r/>');
$m->insertBefore($m->importNode($dtsr_src->doctype), $m->documentElement);
dtsr_try('modern 2nd when 1st child-only',
    fn() => $m->appendChild($m->importNode($dtsr_src->doctype)));
$m = Dom\XMLDocument::createFromString('<r/>');
dtsr_try('modern after an element', fn() => $m->appendChild($m->importNode($dtsr_src->doctype)));
$m = Dom\XMLDocument::createEmpty();
dtsr_try('modern into an empty document',
    fn() => $m->appendChild($m->importNode($dtsr_src->doctype)));
?>
--EXPECT--
append 2nd                             DOMException(3): A document may only contain one document type
insertBefore 2nd                       ok DOMDocumentType
replaceChild over the doctype          ok DOMDocumentType
element receiver                       DOMException(3): A document may only contain one document type
2nd when 1st is child-only             ok DOMDocumentType
re-append the same doctype             ok DOMDocumentType
after an element, no subset yet        ok DOMDocumentType
foreign 2nd doctype                    DOMException(4): Wrong Document Error
before: owner=null
after : owner=set parent=null children=2
warning: DOMNode::appendChild(): A document may only contain one document type
lax receiver                           ok false
modern 2nd                             DOMException(3): Cannot have more than one document type
modern element receiver                DOMException(3): Cannot insert a document type into anything other than a document
modern 2nd when 1st child-only         DOMException(3): Cannot have more than one document type
modern after an element                DOMException(3): Document types must be the first child in a document
modern into an empty document          ok Dom\DocumentType
