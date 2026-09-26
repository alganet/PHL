--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XMLWriter: php's procedural half (all 42 xmlwriter_* names with their signatures, one writer driven from both spellings, and the argument numbering the two share)
--FILE--
<?php
function xw_c6_try(string $label, callable $fn): void {
	try {
		$r = $fn();
		echo $label, ": ";
		var_export($r);
		echo "\n";
	} catch (Throwable $e) {
		echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
	}
}

// Every name of php's procedural half, with the signature each reflects.
$names = [];
foreach (get_defined_functions()['internal'] as $f) {
	if (strncmp($f, 'xmlwriter_', 10) === 0) { $names[] = $f; }
}
sort($names);
foreach ($names as $f) {
	$m = new ReflectionFunction($f);
	$ps = [];
	foreach ($m->getParameters() as $p) {
		$ps[] = ((string)($p->getType() ?? 'mixed')) . ' $' . $p->getName()
			. ($p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
	}
	printf("%s(%s): %s\n", $f, implode(', ', $ps), (string)($m->getReturnType() ?? '?'));
}
printf("%d functions\n", count($names));

// A whole document written through the function spelling.
$w = xmlwriter_open_memory();
printf("open_memory answers %s\n", get_class($w));
xmlwriter_set_indent($w, true);
xmlwriter_start_document($w, '1.0', 'UTF-8');
xmlwriter_start_dtd($w, 'feed');
xmlwriter_write_dtd_entity($w, 'brand', 'Example');
xmlwriter_end_dtd($w);
xmlwriter_start_element_ns($w, 'atom', 'feed', 'http://www.w3.org/2005/Atom');
xmlwriter_write_element($w, 'title', 'News');
xmlwriter_start_element($w, 'entry');
xmlwriter_start_attribute($w, 'id');
xmlwriter_text($w, '1');
xmlwriter_end_attribute($w);
xmlwriter_write_cdata($w, 'a < b');
xmlwriter_write_pi($w, 'render', 'now');
xmlwriter_end_element($w);
xmlwriter_end_element($w);
xmlwriter_end_document($w);
echo xmlwriter_output_memory($w);

// The two spellings drive ONE writer, so they can be mixed.
$m = new XMLWriter;
$m->openMemory();
$m->startElement('a');
xmlwriter_write_attribute($m, 'k', 'v');
$m->text('x');
xmlwriter_end_element($m);
var_export(xmlwriter_output_memory($m));
echo "\n";

// The diagnostics are shared, so the function spelling reports the argument
// numbering php's macro was written for -- and names the real parameter.
$b = xmlwriter_open_memory();
xw_c6_try('proc startElement bad',    fn() => xmlwriter_start_element($b, 'x y'));
xw_c6_try('proc writeElement bad',    fn() => xmlwriter_write_element($b, 'x y', 'c'));
xw_c6_try('proc startElementNs bad',  fn() => xmlwriter_start_element_ns($b, 'p', 'x y', 'urn:x'));
xw_c6_try('proc startPi bad',         fn() => xmlwriter_start_pi($b, 'x y'));
xw_c6_try('proc startDtdElement bad', fn() => xmlwriter_start_dtd_element($b, 'x y'));
xw_c6_try('proc startDtdEntity bad',  fn() => xmlwriter_start_dtd_entity($b, 'x y', false));
xw_c6_try('proc uninit writer',       fn() => xmlwriter_start_element(new XMLWriter, 'a'));
xw_c6_try('proc not a writer',        fn() => xmlwriter_start_element(new stdClass, 'a'));
xw_c6_try('proc null writer',         fn() => xmlwriter_start_element(null, 'a'));
xw_c6_try('proc too few',             fn() => xmlwriter_start_element($b));
xw_c6_try('proc open_uri empty',      fn() => xmlwriter_open_uri(''));

// A file written entirely through functions.
$dir = sys_get_temp_dir() . '/phl_xwp_' . getmypid();
@mkdir($dir);
$path = $dir . '/f.xml';
@unlink($path);
$u = xmlwriter_open_uri($path);
printf("open_uri answers %s\n", get_class($u));
xmlwriter_write_element($u, 'ok', '1');
xmlwriter_flush($u);
printf("file: %s\n", file_get_contents($path));
@unlink($path);
@rmdir($dir);
--EXPECT--
xmlwriter_end_attribute(XMLWriter $writer): bool
xmlwriter_end_cdata(XMLWriter $writer): bool
xmlwriter_end_comment(XMLWriter $writer): bool
xmlwriter_end_document(XMLWriter $writer): bool
xmlwriter_end_dtd(XMLWriter $writer): bool
xmlwriter_end_dtd_attlist(XMLWriter $writer): bool
xmlwriter_end_dtd_element(XMLWriter $writer): bool
xmlwriter_end_dtd_entity(XMLWriter $writer): bool
xmlwriter_end_element(XMLWriter $writer): bool
xmlwriter_end_pi(XMLWriter $writer): bool
xmlwriter_flush(XMLWriter $writer, bool $empty = true): string|int
xmlwriter_full_end_element(XMLWriter $writer): bool
xmlwriter_open_memory(): XMLWriter|false
xmlwriter_open_uri(string $uri): XMLWriter|false
xmlwriter_output_memory(XMLWriter $writer, bool $flush = true): string
xmlwriter_set_indent(XMLWriter $writer, bool $enable): bool
xmlwriter_set_indent_string(XMLWriter $writer, string $indentation): bool
xmlwriter_start_attribute(XMLWriter $writer, string $name): bool
xmlwriter_start_attribute_ns(XMLWriter $writer, ?string $prefix, string $name, ?string $namespace): bool
xmlwriter_start_cdata(XMLWriter $writer): bool
xmlwriter_start_comment(XMLWriter $writer): bool
xmlwriter_start_document(XMLWriter $writer, ?string $version = '1.0', ?string $encoding = NULL, ?string $standalone = NULL): bool
xmlwriter_start_dtd(XMLWriter $writer, string $qualifiedName, ?string $publicId = NULL, ?string $systemId = NULL): bool
xmlwriter_start_dtd_attlist(XMLWriter $writer, string $name): bool
xmlwriter_start_dtd_element(XMLWriter $writer, string $qualifiedName): bool
xmlwriter_start_dtd_entity(XMLWriter $writer, string $name, bool $isParam): bool
xmlwriter_start_element(XMLWriter $writer, string $name): bool
xmlwriter_start_element_ns(XMLWriter $writer, ?string $prefix, string $name, ?string $namespace): bool
xmlwriter_start_pi(XMLWriter $writer, string $target): bool
xmlwriter_text(XMLWriter $writer, string $content): bool
xmlwriter_write_attribute(XMLWriter $writer, string $name, string $value): bool
xmlwriter_write_attribute_ns(XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, string $value): bool
xmlwriter_write_cdata(XMLWriter $writer, string $content): bool
xmlwriter_write_comment(XMLWriter $writer, string $content): bool
xmlwriter_write_dtd(XMLWriter $writer, string $name, ?string $publicId = NULL, ?string $systemId = NULL, ?string $content = NULL): bool
xmlwriter_write_dtd_attlist(XMLWriter $writer, string $name, string $content): bool
xmlwriter_write_dtd_element(XMLWriter $writer, string $name, string $content): bool
xmlwriter_write_dtd_entity(XMLWriter $writer, string $name, string $content, bool $isParam = false, ?string $publicId = NULL, ?string $systemId = NULL, ?string $notationData = NULL): bool
xmlwriter_write_element(XMLWriter $writer, string $name, ?string $content = NULL): bool
xmlwriter_write_element_ns(XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, ?string $content = NULL): bool
xmlwriter_write_pi(XMLWriter $writer, string $target, string $content): bool
xmlwriter_write_raw(XMLWriter $writer, string $content): bool
42 functions
open_memory answers XMLWriter
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE feed [
 <!ENTITY brand "Example">
]>
<atom:feed xmlns:atom="http://www.w3.org/2005/Atom">
 <title>News</title>
 <entry id="1"><![CDATA[a < b]]><?render now?>
</entry>
</atom:feed>
'<a k="v">x</a>'
proc startElement bad: ValueError: xmlwriter_start_element(): Argument #2 ($name) must be a valid element name, "x y" given
proc writeElement bad: ValueError: xmlwriter_write_element(): Argument #2 ($name) must be a valid element name, "x y" given
proc startElementNs bad: ValueError: xmlwriter_start_element_ns(): Argument #3 ($name) must be a valid element name, "x y" given
proc startPi bad: ValueError: xmlwriter_start_pi(): Argument #2 ($target) must be a valid PI target, "x y" given
proc startDtdElement bad: ValueError: xmlwriter_start_dtd_element(): Argument #2 ($qualifiedName) must be a valid element name, "x y" given
proc startDtdEntity bad: ValueError: xmlwriter_start_dtd_entity(): Argument #2 ($name) must be a valid attribute name, "x y" given
proc uninit writer: Error: Invalid or uninitialized XMLWriter object
proc not a writer: TypeError: xmlwriter_start_element(): Argument #1 ($writer) must be of type XMLWriter, stdClass given
proc null writer: TypeError: xmlwriter_start_element(): Argument #1 ($writer) must be of type XMLWriter, null given
proc too few: ArgumentCountError: xmlwriter_start_element() expects exactly 2 arguments, 1 given
proc open_uri empty: ValueError: xmlwriter_open_uri(): Argument #1 ($uri) must not be empty
open_uri answers XMLWriter
file: <ok>1</ok>
