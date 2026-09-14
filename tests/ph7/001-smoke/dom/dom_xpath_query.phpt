--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMXPath query: absolute/predicate/descendant paths, 2-arg context node, document order
--FILE--
<?php
$d = new DOMDocument;
$d->preserveWhiteSpace = false;
$d->loadXML('<r><logging><log type="junit" target="a.xml"/><log type="tap" target="b"/></logging><item id="1"><file>x</file><line>10</line></item><item id="2"><file>y</file></item></r>');
$xp = new DOMXPath($d);
// absolute + predicate
$logs = $xp->query('//logging/log[@type="junit"]');
var_dump($logs->length, $logs->item(0)->getAttribute('target'));
// descendant wildcard
var_dump($xp->query('//item')->length);
// 2-arg context query
$item = $xp->query('//item')->item(0);
$files = $xp->query('file', $item);
var_dump($files->length, $files->item(0)->textContent);
$lines = $xp->query('line', $item);
var_dump($lines->length);
$item2 = $xp->query('//item')->item(1);
var_dump($xp->query('line', $item2)->length);
// no matches
var_dump($xp->query('//nope')->length);
// invalid expression (capture, so no display warning in either runner)
$prev = libxml_use_internal_errors(true);
var_dump($xp->query('///bad['));
libxml_use_internal_errors($prev);
libxml_clear_errors();
// iterate result
foreach ($xp->query('//item') as $i => $n) { echo $i, ':', $n->getAttribute('id'), ' '; }
echo "\n";
// document order
$d2 = new DOMDocument;
$d2->loadXML('<r><a/><b><a/></b><a/></r>');
$xp2 = new DOMXPath($d2);
$res = $xp2->query('//a');
var_dump($res->length);
foreach ($res as $n) { echo $n->getLineNo(), ':'; }
echo "\n";
--EXPECT--
int(1)
string(5) "a.xml"
int(2)
int(1)
string(1) "x"
int(1)
int(0)
int(0)
bool(false)
0:1 1:2 
int(3)
1:1:1:
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
