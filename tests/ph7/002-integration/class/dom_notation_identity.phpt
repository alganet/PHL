--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a notation is one node per DECLARATION, so two lookups answer one object (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* an xmlNotation is three strings with no type field, so both engines answer
 * one through a stand-in NODE built around it. php builds a fresh one per
 * lookup and frees it with the object; PHL builds one per declaration and
 * keeps it on the document, which is what every other node here does -- so the
 * identity DOM guarantees elsewhere holds for these too. The VALUES are the
 * same either way; only the object identity differs. */
$d = new DOMDocument;
$d->loadXML('<!DOCTYPE r [<!NOTATION n SYSTEM "v">]><r/>');
$m = $d->doctype->notations;
var_dump($m->item(0) === $m->item(0), $m->getNamedItem('n') === $m->getNamedItem('n'),
    $m->item(0) === $m->getNamedItem('n'), $m->item(0)->systemId);
--EXPECT--
bool(true)
bool(true)
bool(true)
string(1) "v"
--CLEAN--
<?php
