--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XMLWriter: the DTD twelve (DOCTYPE, internal subset element/attlist/entity declarations, and php's two calls behind writeDtdEntity)
--FILE--
<?php
function xw_c4_try(string $label, callable $fn): void {
	try {
		$r = $fn();
		echo $label, ": ";
		var_export($r);
		echo "\n";
	} catch (Throwable $e) {
		echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
	}
}
function xw_c4_new(): XMLWriter {
	$w = new XMLWriter;
	$w->openMemory();
	return $w;
}
// libxml explains two of its DTD refusals through the error handler only; report
// the LEVEL, since its wording moves between library versions.
set_error_handler(function (int $no, string $msg): bool {
	echo "  [warning raised]\n";
	return true;
});

// A document with a public DOCTYPE and an internal subset built in pieces.
$w = xw_c4_new();
$w->startDocument('1.0', 'UTF-8');
$w->startDtd('html', '-//W3C//DTD XHTML 1.0 Strict//EN', 'http://www.w3.org/TR/xhtml1.dtd');
$w->startDtdElement('page');
$w->text('(title, body)');
$w->endDtdElement();
$w->startDtdAttlist('page');
$w->text('id ID #IMPLIED');
$w->endDtdAttlist();
$w->startDtdEntity('company', false);
$w->text('Example Ltd');
$w->endDtdEntity();
$w->writeDtdEntity('percent', '&#37;', true);
$w->endDtd();
$w->startElement('html');
$w->endElement();
$w->endDocument();
echo $w->outputMemory();

// The written forms of the same four.
$w2 = xw_c4_new();
$w2->writeDtd('root', 'pub', 'sys', '<!ELEMENT x EMPTY>');
$w2->writeDtdElement('e', '(#PCDATA)');
$w2->writeDtdAttlist('e', 'a CDATA #IMPLIED');
$w2->writeDtdEntity('inner', 'v');
var_export($w2->outputMemory());
echo "\n";

// php has two calls behind writeDtdEntity: with an identifier the declaration is
// EXTERNAL and $content is not written at all.
foreach ([
	['e', 'c', false, null, null, null],
	['e', 'c', true,  null, null, null],
	['e', 'c', false, null, 'sys', null],
	['e', 'c', false, 'pub', 'sys', null],
	['e', 'c', false, 'pub', 'sys', 'note'],
	['e', 'c', false, null, 'sys', 'note'],
	['e', 'c', false, null, null, 'note'],
] as [$n, $c, $p, $pub, $sys, $nd]) {
	$w3 = xw_c4_new();
	$r = $w3->writeDtdEntity($n, $c, $p, $pub, $sys, $nd);
	printf("entity(%s,%s,%s,%s) = %s %s\n", var_export($p, true), var_export($pub, true),
		var_export($sys, true), var_export($nd, true), var_export($r, true), $w3->outputMemory());
}

// A DOCTYPE with only a public identifier, and a DTD once the root is open:
// libxml refuses both through its error handler.
xw_c4_try('startDtd public only', function () {
	$w = xw_c4_new();
	$r = $w->startDtd('h', 'pub');
	return [$r, $w->outputMemory()];
});
xw_c4_try('startDtd inside element', function () {
	$w = xw_c4_new();
	$w->startElement('a');
	return $w->startDtd('h');
});

// Unbalanced ends, one answer each.
xw_c4_try('endDtd unbalanced',        fn() => xw_c4_new()->endDtd());
xw_c4_try('endDtdElement unbalanced', fn() => xw_c4_new()->endDtdElement());
xw_c4_try('endDtdAttlist unbalanced', fn() => xw_c4_new()->endDtdAttlist());
xw_c4_try('endDtdEntity unbalanced',  fn() => xw_c4_new()->endDtdEntity());

// The DOCTYPE's own qualified name is NOT validated (php checks the internal
// declarations only), and startDtdEntity's ValueError says "attribute name"
// where its neighbours say "element name" -- php reaches for another macro.
xw_c4_try('startDtd bad name', function () {
	$w = xw_c4_new();
	$r = $w->startDtd('x y');
	return [$r, $w->outputMemory()];
});
xw_c4_try('startDtd empty name', function () {
	$w = xw_c4_new();
	$r = $w->startDtd('');
	return [$r, $w->outputMemory()];
});
xw_c4_try('startDtdElement bad', fn() => xw_c4_new()->startDtdElement('x y'));
xw_c4_try('writeDtdElement bad', fn() => xw_c4_new()->writeDtdElement('x y', 'c'));
xw_c4_try('startDtdAttlist bad', fn() => xw_c4_new()->startDtdAttlist('x y'));
xw_c4_try('writeDtdAttlist bad', fn() => xw_c4_new()->writeDtdAttlist('x y', 'c'));
xw_c4_try('startDtdEntity bad',  fn() => xw_c4_new()->startDtdEntity('x y', false));
xw_c4_try('writeDtdEntity bad',  fn() => xw_c4_new()->writeDtdEntity('x y', 'c'));
restore_error_handler();
--EXPECT--
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1.dtd" [<!ELEMENT page (title, body)><!ATTLIST page id ID #IMPLIED><!ENTITY company "Example Ltd"><!ENTITY % percent "&#37;">]><html/>
'<!DOCTYPE root PUBLIC "pub" "sys" [<!ELEMENT x EMPTY>]><!ELEMENT e (#PCDATA)><!ATTLIST e a CDATA #IMPLIED><!ENTITY inner "v">'
entity(false,NULL,NULL,NULL) = true <!ENTITY e "c">
entity(true,NULL,NULL,NULL) = true <!ENTITY % e "c">
entity(false,NULL,'sys',NULL) = true <!ENTITY e SYSTEM "sys">
entity(false,'pub','sys',NULL) = true <!ENTITY e PUBLIC "pub" "sys">
entity(false,'pub','sys','note') = true <!ENTITY e PUBLIC "pub" "sys" NDATA note>
entity(false,NULL,'sys','note') = true <!ENTITY e SYSTEM "sys" NDATA note>
entity(false,NULL,NULL,'note') = true <!ENTITY e "c">
  [warning raised]
startDtd public only: array (
  0 => false,
  1 => '<!DOCTYPE h',
)
  [warning raised]
startDtd inside element: false
endDtd unbalanced: true
endDtdElement unbalanced: false
endDtdAttlist unbalanced: false
endDtdEntity unbalanced: false
startDtd bad name: array (
  0 => true,
  1 => '<!DOCTYPE x y',
)
startDtd empty name: array (
  0 => false,
  1 => '',
)
startDtdElement bad: ValueError: XMLWriter::startDtdElement(): Argument #2 must be a valid element name, "x y" given
writeDtdElement bad: ValueError: XMLWriter::writeDtdElement(): Argument #2 ($content) must be a valid element name, "x y" given
startDtdAttlist bad: ValueError: XMLWriter::startDtdAttlist(): Argument #2 must be a valid element name, "x y" given
writeDtdAttlist bad: ValueError: XMLWriter::writeDtdAttlist(): Argument #2 ($content) must be a valid element name, "x y" given
startDtdEntity bad: ValueError: XMLWriter::startDtdEntity(): Argument #2 ($isParam) must be a valid attribute name, "x y" given
writeDtdEntity bad: ValueError: XMLWriter::writeDtdEntity(): Argument #2 ($content) must be a valid element name, "x y" given
