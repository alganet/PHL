--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A canonicalization that FAILS answers false, not the empty string
--FILE--
<?php
// Only the FIRST diagnostic is pinned: libxml 2.9 follows it with two
// "Internal error : processing ... list" lines that 2.13 does not emit, so the
// count of them has no cross-version answer.
$dom_c14n_said = [];
set_error_handler(static function (int $n, string $s) use (&$dom_c14n_said): bool {
    $dom_c14n_said[] = sprintf("[E%d] %s", $n, $s);
    return true;
});
$dom_c14n_first = static function () use (&$dom_c14n_said): string {
    $first = $dom_c14n_said[0] ?? '(nothing said)';
    $dom_c14n_said = [];
    return $first;
};

// An entity REFERENCE cannot be canonicalized, and an ordinary document read
// without substituteEntities has one -- so this is not an exotic input. php
// answers false and says why; answering "" would have a signer sign the empty
// string and never learn the document was not canonicalized at all.
$ref = new DOMDocument;
$ref->substituteEntities = false;
$ref->loadXML('<!DOCTYPE r [<!ENTITY a "A">]><r>&a;</r>');
var_dump($ref->C14N());
echo $dom_c14n_first(), "\n";
var_dump($ref->documentElement->C14N());
echo $dom_c14n_first(), "\n";

// ...while a canonicalization that succeeds with NO bytes is a real answer:
// nothing of a detached node or a fragment is visible from the document.
$d = new DOMDocument;
$d->substituteEntities = false;
$d->loadXML('<!DOCTYPE r [<!ENTITY a "A">]><r><k/></r>');
var_dump($d->C14N(), $d->documentElement->C14N(),
    $d->createElement('x')->C14N(), $d->createDocumentFragment()->C14N(),
    $d->doctype->C14N(), $d->documentElement->firstChild->C14N());

// The file spelling answers the same failure, and the byte count otherwise.
$path = tempnam(sys_get_temp_dir(), 'dom_c14n_');
var_dump($ref->C14NFile($path));
echo $dom_c14n_first(), "\n";
var_dump($d->C14NFile($path), file_get_contents($path));
var_dump($d->createElement('x')->C14NFile($path), file_get_contents($path));
unlink($path);
?>
--EXPECT--
bool(false)
[E2] DOMNode::C14N(): Node XML_ENTITY_REF_NODE is invalid here : processing node
bool(false)
[E2] DOMNode::C14N(): Node XML_ENTITY_REF_NODE is invalid here : processing node
string(14) "<r><k></k></r>"
string(14) "<r><k></k></r>"
string(0) ""
string(0) ""
string(0) ""
string(7) "<k></k>"
bool(false)
[E2] DOMNode::C14NFile(): Node XML_ENTITY_REF_NODE is invalid here : processing node
int(14)
string(14) "<r><k></k></r>"
int(0)
string(0) ""
