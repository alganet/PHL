--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XMLWriter: the attribute pair and the namespace half (startAttribute/endAttribute, startElementNs/writeElementNs, startAttributeNs/writeAttributeNs)
--FILE--
<?php
function xw_c2_try(string $label, callable $fn): void {
	try {
		$r = $fn();
		echo $label, ": ";
		var_export($r);
		echo "\n";
	} catch (Throwable $e) {
		echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
	}
}
function xw_c2_new(): XMLWriter {
	$w = new XMLWriter;
	$w->openMemory();
	return $w;
}

// The attribute pair: a value built in pieces, escaped the way a written one is.
$w = xw_c2_new();
$w->startElement('a');
$w->startAttribute('x');
$w->text('v&1');
$w->text('<2>');
$w->endAttribute();
$w->text('body');
$w->endElement();
var_export($w->outputMemory());
echo "\n";

// Out of place, both halves answer false and write nothing.
xw_c2_try('startAttribute outside element', fn() => xw_c2_new()->startAttribute('x'));
xw_c2_try('endAttribute unbalanced',        fn() => xw_c2_new()->endAttribute());
$w2 = xw_c2_new();
$w2->startElement('e');
$w2->text('t');
xw_c2_try('writeAttribute after text', fn() => $w2->writeAttribute('a', 'v'));

// A namespaced document: the prefix is declared on the element that opens it.
$w3 = xw_c2_new();
$w3->startDocument('1.0', 'UTF-8');
$w3->startElementNs('soap', 'Envelope', 'http://schemas.xmlsoap.org/soap/envelope/');
$w3->writeAttributeNs('soap', 'encodingStyle', 'http://schemas.xmlsoap.org/soap/envelope/', 'http://x');
$w3->startElementNs('soap', 'Body', null);
$w3->writeElementNs(null, 'plain', 'urn:inner', 'v');
$w3->startAttributeNs('p', 'attr', 'urn:p');
$w3->text('av');
$w3->endAttribute();
$w3->endElement();
$w3->endElement();
$w3->endDocument();
var_export($w3->outputMemory());
echo "\n";

// null prefix declares the DEFAULT namespace; null namespace declares nothing.
foreach ([['p', 'urn:x'], [null, 'urn:x'], ['p', null], [null, null]] as [$prefix, $ns]) {
	$w4 = xw_c2_new();
	$w4->startElementNs($prefix, 'e', $ns);
	$w4->endElement();
	printf("ns(%s,%s) = %s\n", var_export($prefix, true), var_export($ns, true), $w4->outputMemory());
}

// writeElementNs with no content is the empty element; with "" it is a pair.
$w5 = xw_c2_new();
$w5->writeElementNs('p', 'e', 'urn:x');
$w5->writeElementNs('p', 'f', 'urn:x', '');
var_export($w5->outputMemory());
echo "\n";

// Only the local name is validated -- php checks neither prefix nor namespace,
// and reports the name as the argument at the PROCEDURAL position.
$w6 = xw_c2_new();
xw_c2_try('startElementNs bad name',    fn() => $w6->startElementNs('p', 'x y', 'urn:x'));
xw_c2_try('writeElementNs bad name',    fn() => $w6->writeElementNs('p', '', 'urn:x', 'c'));
xw_c2_try('startAttributeNs bad name',  fn() => $w6->startAttributeNs('p', '1bad', 'urn:x'));
xw_c2_try('writeAttributeNs bad name',  fn() => $w6->writeAttributeNs('p', 'x y', 'urn:x', 'v'));
xw_c2_try('startAttribute bad name',    fn() => $w6->startAttribute('x y'));
xw_c2_try('bad prefix is written',      function () use ($w6) {
	$r = $w6->startElementNs('x y', 'e', 'urn:x');
	return [$r, $w6->outputMemory()];
});
--EXPECT--
'<a x="v&amp;1&lt;2&gt;">body</a>'
startAttribute outside element: false
endAttribute unbalanced: false
writeAttribute after text: false
'<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope soap:encodingStyle="http://x" xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"><soap:Body><plain xmlns="urn:inner">v</plain>av</soap:Body></soap:Envelope>
'
ns('p','urn:x') = <p:e xmlns:p="urn:x"/>
ns(NULL,'urn:x') = <e xmlns="urn:x"/>
ns('p',NULL) = <p:e/>
ns(NULL,NULL) = <e/>
'<p:e xmlns:p="urn:x"/><p:f xmlns:p="urn:x"></p:f>'
startElementNs bad name: ValueError: XMLWriter::startElementNs(): Argument #3 ($namespace) must be a valid element name, "x y" given
writeElementNs bad name: ValueError: XMLWriter::writeElementNs(): Argument #3 ($namespace) must be a valid element name, "" given
startAttributeNs bad name: ValueError: XMLWriter::startAttributeNs(): Argument #3 ($namespace) must be a valid attribute name, "1bad" given
writeAttributeNs bad name: ValueError: XMLWriter::writeAttributeNs(): Argument #3 ($namespace) must be a valid attribute name, "x y" given
startAttribute bad name: ValueError: XMLWriter::startAttribute(): Argument #2 must be a valid attribute name, "x y" given
bad prefix is written: array (
  0 => true,
  1 => '<x y:e',
)
