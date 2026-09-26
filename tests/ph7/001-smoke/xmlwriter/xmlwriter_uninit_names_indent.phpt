--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XMLWriter: an unopened writer is an Error, an invalid name is a ValueError (php's procedural argument numbering), setIndentString survives setIndent, and the writer is uncloneable
--FILE--
<?php
function xw_c1_try(string $label, callable $fn): void {
	try {
		$r = $fn();
		echo $label, ": ";
		var_export($r);
		echo "\n";
	} catch (Throwable $e) {
		echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
	}
}
// A writer that was never opened has nothing behind it: php refuses every call.
$u = new XMLWriter;
xw_c1_try('uninit startElement', fn() => $u->startElement('a'));
xw_c1_try('uninit text',         fn() => $u->text('x'));
xw_c1_try('uninit writeElement', fn() => $u->writeElement('a', 'b'));
xw_c1_try('uninit outputMemory', fn() => $u->outputMemory());
xw_c1_try('uninit flush',        fn() => $u->flush());
xw_c1_try('uninit setIndent',    fn() => $u->setIndent(true));

// A name libxml will not quote is refused before anything is written -- and the
// argument php names is the PROCEDURAL one, so a method reports "#2" and prints
// whichever of its own parameters sits there (or none at all).
$w = new XMLWriter;
$w->openMemory();
xw_c1_try('startElement bad',    fn() => $w->startElement('1bad'));
xw_c1_try('startElement space',  fn() => $w->startElement('x y'));
xw_c1_try('startElement empty',  fn() => $w->startElement(''));
xw_c1_try('writeElement bad',    fn() => $w->writeElement('x y', 'c'));
xw_c1_try('writeAttribute bad',  function () use ($w) { $w->startElement('a'); return $w->writeAttribute('x y', 'v'); });
echo "after the refusals: ";
var_export($w->outputMemory());
echo "\n";

// A colon is part of XML's Name production, so a prefixed name is accepted.
$w2 = new XMLWriter;
$w2->openMemory();
var_export($w2->startElement('ns:tag'));
echo " ";
var_export($w2->writeAttribute('xmlns:ns', 'urn:x'));
echo " ";
$w2->endElement();
var_export($w2->outputMemory());
echo "\n";

// setIndentString() before setIndent() is the documented order and keeps.
$w3 = new XMLWriter;
$w3->openMemory();
$w3->setIndentString("\t");
$w3->setIndent(true);
$w3->startElement('a');
$w3->startElement('b');
$w3->endElement();
$w3->endElement();
var_export($w3->outputMemory());
echo "\n";

// ... and setIndent() alone still indents with php's single space.
$w4 = new XMLWriter;
$w4->openMemory();
$w4->setIndent(true);
$w4->startElement('a');
$w4->startElement('b');
$w4->endElement();
$w4->endElement();
var_export($w4->outputMemory());
echo "\n";

// The copy would carry the same libxml writer, so php has no clone handler.
xw_c1_try('clone', fn() => clone $w4);
--EXPECT--
uninit startElement: Error: Invalid or uninitialized XMLWriter object
uninit text: Error: Invalid or uninitialized XMLWriter object
uninit writeElement: Error: Invalid or uninitialized XMLWriter object
uninit outputMemory: Error: Invalid or uninitialized XMLWriter object
uninit flush: Error: Invalid or uninitialized XMLWriter object
uninit setIndent: Error: Invalid or uninitialized XMLWriter object
startElement bad: ValueError: XMLWriter::startElement(): Argument #2 must be a valid element name, "1bad" given
startElement space: ValueError: XMLWriter::startElement(): Argument #2 must be a valid element name, "x y" given
startElement empty: ValueError: XMLWriter::startElement(): Argument #2 must be a valid element name, "" given
writeElement bad: ValueError: XMLWriter::writeElement(): Argument #2 ($content) must be a valid element name, "x y" given
writeAttribute bad: ValueError: XMLWriter::writeAttribute(): Argument #2 ($value) must be a valid attribute name, "x y" given
after the refusals: '<a'
true true '<ns:tag xmlns:ns="urn:x"/>'
'<a>
	<b/>
</a>
'
'<a>
 <b/>
</a>
'
clone: Error: Trying to clone an uncloneable object of class XMLWriter
