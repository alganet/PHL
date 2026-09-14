--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
C14N + normalizeDocument + formatOutput: the sebastian/comparator equality flow
--FILE--
<?php
function nodeToText($node) {
    $document = new DOMDocument;
    try { $c14n = $node->C14N(); @$document->loadXML($c14n); } catch (ValueError) {}
    $node = $document;
    $document->formatOutput = true;
    $document->normalizeDocument();
    return $node->saveXML();
}
$a = new DOMDocument; $a->preserveWhiteSpace = false;
$a->loadXML("<root>\n  <child attr='x'>text</child>\n  <other/>\n</root>");
$b = new DOMDocument; $b->preserveWhiteSpace = false;
$b->loadXML('<root><child  attr="x" >text</child><other></other></root>');
var_dump(nodeToText($a) === nodeToText($b));
echo nodeToText($a);
$c = new DOMDocument;
$c->loadXML('<x b="2" a="1"><y/></x>');
echo $c->C14N(), "\n";
echo $c->documentElement->firstChild->C14N(), "\n";
$empty = new DOMDocument;
var_dump($empty->C14N());
--EXPECT--
bool(true)
<?xml version="1.0"?>
<root>
  <child attr="x">text</child>
  <other/>
</root>
<x a="1" b="2"><y></y></x>
<y></y>
string(0) ""
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
