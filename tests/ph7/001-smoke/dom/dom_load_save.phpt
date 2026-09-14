--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMDocument loadXML/saveXML, preserveWhiteSpace, formatOutput, empty-source ValueError
--FILE--
<?php
$d = new DOMDocument;
var_dump($d->loadXML('<r a="1"><x>t</x></r>'));
echo $d->saveXML();
$d->formatOutput = true;
echo $d->saveXML();
echo $d->saveXML($d->documentElement), "\n";
$p = new DOMDocument;
$p->preserveWhiteSpace = false;
$p->loadXML("<r>\n  <a>1</a>\n  <b/>\n</r>");
var_dump($p->documentElement->childNodes->length);
$q = new DOMDocument;
$q->loadXML("<r>\n  <a>1</a>\n  <b/>\n</r>");
var_dump($q->documentElement->childNodes->length);
try { $d->loadXML(''); } catch (ValueError $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$prev = libxml_use_internal_errors(true);
var_dump($d->loadXML('<broken>'));
libxml_use_internal_errors($prev);
$empty = new DOMDocument;
var_dump($empty->documentElement);
--EXPECT--
bool(true)
<?xml version="1.0"?>
<r a="1"><x>t</x></r>
<?xml version="1.0"?>
<r a="1">
  <x>t</x>
</r>
<r a="1">
  <x>t</x>
</r>
int(2)
int(5)
ValueError: DOMDocument::loadXML(): Argument #1 ($source) must not be empty
bool(false)
NULL
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
