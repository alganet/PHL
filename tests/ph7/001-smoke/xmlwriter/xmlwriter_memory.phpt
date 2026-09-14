--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XMLWriter memory API: indent, attributes, cdata/comment, outputMemory flush semantics (PHPUnit Baseline Writer)
--FILE--
<?php
// Mirror PHPUnit's Runner\Baseline\Writer
$w = new XMLWriter;
$w->openMemory();
$w->setIndent(true);
$w->startDocument();
$w->startElement('files');
  $w->startElement('file');
  $w->writeAttribute('path', 'src/Foo.php');
    $w->startElement('line');
    $w->writeAttribute('number', '10');
    $w->writeAttribute('hash', 'abc123');
      $w->startElement('issue');
      $w->text('Something');
      $w->endElement();
    $w->endElement();
  $w->endElement();
$w->endElement();
echo $w->outputMemory();
echo "====\n";
// without indent, with cdata and comment
$w2 = new XMLWriter;
$w2->openMemory();
$w2->startDocument('1.0', 'UTF-8');
$w2->startElement('root');
$w2->writeAttribute('a', 'x&y');
$w2->writeElement('child', 'val');
$w2->writeCdata('c<d>');
$w2->writeComment('note');
$w2->endElement();
$w2->endDocument();
var_export($w2->outputMemory());
echo "\n====\n";
// flush semantics + outputMemory($flush=false)
$w3 = new XMLWriter;
$w3->openMemory();
$w3->writeElement('a', '1');
$first = $w3->outputMemory(false);
$w3->writeElement('b', '2');
$second = $w3->outputMemory();
var_export($first); echo "\n"; var_export($second); echo "\n";
--EXPECT--
<?xml version="1.0"?>
<files>
 <file path="src/Foo.php">
  <line number="10" hash="abc123">
   <issue>Something</issue>
  </line>
 </file>
</files>
====
'<?xml version="1.0" encoding="UTF-8"?>
<root a="x&amp;y"><child>val</child><![CDATA[c<d>]]><!--note--></root>
'
====
'<a>1</a>'
'<a>1</a><b>2</b>'
