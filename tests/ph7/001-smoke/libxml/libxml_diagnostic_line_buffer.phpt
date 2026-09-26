--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A libxml message with no trailing newline is held back and printed joined to the next
--FILE--
<?php
// Reset shared state (the in-process smoke runner shares one interpreter)
libxml_use_internal_errors(false);
libxml_clear_errors();
set_error_handler(function ($no, $str) { echo "diag $no: ", str_replace("\n", '', $str), "\n"; return true; });

// libxml's "Validation failed: no DTD found !" carries no trailing newline, and
// php's diagnostics are line-buffered: nothing is printed for this parse.
$a = new DOMDocument;
$a->validateOnParse = true;
var_dump($a->loadXML('<r/>'));

// It is still held when the next parse -- a different document -- produces one
// that DOES end in a newline, and the two are printed as one diagnostic, with
// the second one's location.
$b = new DOMDocument;
$b->validateOnParse = true;
var_dump($b->loadXML('<!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>'));

// Fragments accumulate: three held messages come out in front of one flush.
foreach ([1, 2, 3] as $i) {
    $c = new DOMDocument;
    $c->validateOnParse = true;
    $c->loadXML('<r/>');
}
echo "held\n";
$d = new DOMDocument;
$d->validateOnParse = true;
$d->loadXML('<!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>');

// The QUEUE is not line-buffered: libxml_get_errors() reports each message as
// libxml raised it, newline and all.
libxml_use_internal_errors(true);
$q = new DOMDocument;
$q->validateOnParse = true;
$q->loadXML('<r/>');
foreach (libxml_get_errors() as $e) {
    echo 'queued code=', $e->code, ' msg=', str_replace("\n", '\\n', $e->message), "\n";
}
libxml_clear_errors();
libxml_use_internal_errors(false);
--EXPECT--
bool(true)
diag 2: DOMDocument::loadXML(): Validation failed: no DTD found !No declaration for element b in Entity, line: 1
diag 2: DOMDocument::loadXML(): Element r content does not follow the DTD, expecting (a), got (b) in Entity, line: 1
bool(true)
held
diag 2: DOMDocument::loadXML(): Validation failed: no DTD found !Validation failed: no DTD found !Validation failed: no DTD found !No declaration for element b in Entity, line: 1
diag 2: DOMDocument::loadXML(): Element r content does not follow the DTD, expecting (a), got (b) in Entity, line: 1
queued code=522 msg=Validation failed: no DTD found !
