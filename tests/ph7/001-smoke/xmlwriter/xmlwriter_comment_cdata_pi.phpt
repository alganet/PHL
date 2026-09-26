--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XMLWriter: the comment, CDATA and processing-instruction pairs (startComment/endComment, startCdata/endCdata, startPi/endPi/writePi)
--FILE--
<?php
function xw_c3_try(string $label, callable $fn): void {
	try {
		$r = $fn();
		echo $label, ": ";
		var_export($r);
		echo "\n";
	} catch (Throwable $e) {
		echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
	}
}
function xw_c3_new(): XMLWriter {
	$w = new XMLWriter;
	$w->openMemory();
	return $w;
}
// libxml explains some refusals through its error handler only; php turns each
// into a warning naming the caller. Report the LEVEL, not libxml's own wording,
// which moves between library versions.
set_error_handler(function (int $no, string $msg): bool {
	echo "  [warning raised]\n";
	return true;
});

// A document with all three: a comment, a CDATA section and a processing
// instruction, each built as a pair around text() calls.
$w = xw_c3_new();
$w->startDocument('1.0', 'UTF-8');
$w->writePi('xml-stylesheet', 'type="text/xsl" href="s.xsl"');
$w->startElement('page');
$w->startComment();
$w->text('generated');
$w->endComment();
$w->startCdata();
$w->text('if (a < b && c) { }');
$w->endCdata();
$w->startPi('php');
$w->text('echo 1;');
$w->endPi();
$w->endElement();
$w->endDocument();
var_export($w->outputMemory());
echo "\n";

// The written forms, and what each pair does with a payload it cannot escape.
$w2 = xw_c3_new();
$w2->writeCdata('a]]>b');
$w2->writeComment('a--b');
$w2->writePi('t', '');
var_export($w2->outputMemory());
echo "\n";

// Out of place, each half answers for itself.
xw_c3_try('endComment unbalanced', fn() => xw_c3_new()->endComment());
xw_c3_try('endCdata unbalanced',   fn() => xw_c3_new()->endCdata());
xw_c3_try('endPi unbalanced',      fn() => xw_c3_new()->endPi());
xw_c3_try('nested startComment',   function () {
	$w = xw_c3_new();
	$a = $w->startComment();
	$b = $w->startComment();
	$w->text('x');
	$w->endComment();
	return [$a, $b, $w->outputMemory()];
});
xw_c3_try('nested startCdata', function () {
	$w = xw_c3_new();
	$a = $w->startCdata();
	$b = $w->startCdata();
	$w->endCdata();
	return [$a, $b, $w->outputMemory()];
});

// A PI target goes through the same name check the elements do -- php calls it
// a "PI target" and reports it at the procedural argument position.
xw_c3_try('startPi bad target', fn() => xw_c3_new()->startPi('x y'));
xw_c3_try('writePi bad target', fn() => xw_c3_new()->writePi('', 'c'));
xw_c3_try('startPi reserved xml', function () {
	$w = xw_c3_new();
	$r = $w->startPi('xml');
	return [$r, $w->outputMemory()];
});
restore_error_handler();
--EXPECT--
'<?xml version="1.0" encoding="UTF-8"?>
<?xml-stylesheet type="text/xsl" href="s.xsl"?><page><!--generated--><![CDATA[if (a < b && c) { }]]><?php echo 1;?></page>
'
'<![CDATA[a]]>b]]><!--a--b--><?t ?>'
endComment unbalanced: false
endCdata unbalanced: false
endPi unbalanced: true
nested startComment: array (
  0 => true,
  1 => false,
  2 => '<!--x-->',
)
  [warning raised]
nested startCdata: array (
  0 => true,
  1 => false,
  2 => '<![CDATA[]]>',
)
startPi bad target: ValueError: XMLWriter::startPi(): Argument #2 must be a valid PI target, "x y" given
writePi bad target: ValueError: XMLWriter::writePi(): Argument #2 ($content) must be a valid PI target, "" given
  [warning raised]
startPi reserved xml: array (
  0 => false,
  1 => '',
)
