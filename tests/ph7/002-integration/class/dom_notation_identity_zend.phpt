--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a notation stand-in node is built per LOOKUP, so two lookups answer two objects (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php's create_notation() runs on every item()/getNamedItem(), and the object
 * it hands back OWNS the node it built */
$d = new DOMDocument;
$d->loadXML('<!DOCTYPE r [<!NOTATION n SYSTEM "v">]><r/>');
$m = $d->doctype->notations;
var_dump($m->item(0) === $m->item(0), $m->getNamedItem('n') === $m->getNamedItem('n'),
    $m->item(0) === $m->getNamedItem('n'), $m->item(0)->systemId);
--EXPECT--
bool(false)
bool(false)
bool(false)
string(1) "v"
--CLEAN--
<?php
