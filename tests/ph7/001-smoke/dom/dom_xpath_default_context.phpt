--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMXPath::query with no context node evaluates relative paths against the document element
--FILE--
<?php
// DOMXPath::query with NO context node evaluates relative paths against the
// document ELEMENT (php semantics), so a relative name matches root's children.
$d = new DOMDocument;
$d->loadXML('<files version="1"><file path="a"><line n="1"/></file><file path="b"/></files>');
$xp = new DOMXPath($d);
var_dump($xp->query('file')->length);   // children of <files>
var_dump($xp->query('files')->length);  // no such child
var_dump($xp->query('/files')->length); // absolute
var_dump($xp->query('/files/file')->length);
var_dump($xp->query('//line')->length); // descendant
// explicit context still wins
$first = $xp->query('file')->item(0);
var_dump($xp->query('line', $first)->length);
var_dump($xp->query('line', $xp->query('file')->item(1))->length);
foreach ($xp->query('file') as $f) { echo $f->getAttribute('path'); }
echo "\n";
--EXPECT--
int(2)
int(0)
int(1)
int(2)
int(1)
int(1)
int(0)
ab
