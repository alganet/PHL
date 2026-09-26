--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
XMLWriter: writing to a URI or a stream (openUri, php 8.5's toMemory/toUri/toStream factories, the flush the object's release performs, the two failures toUri tells apart, and the two arguments a NUL is refused in)
--FILE--
<?php
function xw_c5_try(string $label, callable $fn): void {
	try {
		$r = $fn();
		echo $label, ": ";
		var_export($r);
		echo "\n";
	} catch (Throwable $e) {
		echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
	}
}
$dir = sys_get_temp_dir() . '/phl_xw_' . getmypid();
@mkdir($dir);
$path = $dir . '/out.xml';
@unlink($path);

// A writer on a file: the bytes go through the engine's stream layer, and the
// document is complete once the writer is flushed.
$w = new XMLWriter;
var_export($w->openUri($path));
echo "\n";
$w->setIndent(true);
$w->startDocument('1.0', 'UTF-8');
$w->startElement('urlset');
$w->writeElement('loc', 'https://example.test/');
$w->endElement();
$w->endDocument();
$w->flush();
echo file_get_contents($path);

// The writer is freed with the object, and that is what finishes the file --
// nothing else wrote it, and the memory reader answers "" for a URI writer.
$path2 = $dir . '/late.xml';
@unlink($path2);
$w2 = new XMLWriter;
$w2->openUri($path2);
$w2->writeElement('a', '1');
printf("before release: %s / outputMemory %s\n",
	var_export(file_get_contents($path2), true), var_export($w2->outputMemory(), true));
unset($w2);
printf("after release: %s\n", var_export(file_get_contents($path2), true));

// php 8.5's three factories answer a NEW writer of the LATE STATIC class.
$m = XMLWriter::toMemory();
$m->writeElement('a', '1');
printf("toMemory %s %s\n", get_class($m), $m->outputMemory());

$path3 = $dir . '/tou.xml';
@unlink($path3);
$u = XMLWriter::toUri($path3);
$u->writeElement('b', '2');
$u->flush();
printf("toUri %s %s\n", get_class($u), file_get_contents($path3));

$h = fopen($dir . '/tos.xml', 'w');
$s = XMLWriter::toStream($h);
$s->writeElement('c', '3');
$s->flush();
fclose($h);
printf("toStream %s %s\n", get_class($s), file_get_contents($dir . '/tos.xml'));

class XwC5Kid extends XMLWriter {}
$k = XwC5Kid::toMemory();
$k->writeElement('d', '4');
printf("subclass %s %s\n", get_class($k), $k->outputMemory());
printf("static through an instance: %s\n", get_class($m->toMemory()));

// A memory stream is a destination like any other, so a writer can be read back
// without ever touching the filesystem.
$mem = fopen('php://memory', 'w+');
$ms = XMLWriter::toStream($mem);
$ms->writeElement('e', '5');
$ms->flush();
rewind($mem);
printf("php://memory %s\n", stream_get_contents($mem));

// What each refusal answers.
xw_c5_try('openUri empty',      fn() => (new XMLWriter)->openUri(''));
xw_c5_try('openUri nul',        fn() => (new XMLWriter)->openUri("a\0b"));
xw_c5_try('toUri nul',          fn() => XMLWriter::toUri("a\0b"));
xw_c5_try('toStream int',       fn() => XMLWriter::toStream(42));
xw_c5_try('toStream closed',    function () { $x = fopen('php://memory', 'w'); fclose($x); return XMLWriter::toStream($x); });
// The warnings a failed open raises name absolute paths, and php adds libxml's
// own text to them; the EXCEPTION each spelling raises is the
// contract, so the warnings are swallowed here.
set_error_handler(fn (int $no, string $msg): bool => true);
// A path php cannot write raises two different things from the FACTORY: the
// ValueError when the path did not RESOLVE, and php's plain Error when the open
// itself was refused. The opener answers false for both.
foreach ([
	// The absolute path is written as a file:/// URI: php reads the path as a URI
	// first, and a Windows drive path ("C:") parses as a SCHEME, which skips the
	// directory check and answers the plain Error -- as any scheme does. file:///
	// is stripped and checked on every platform.
	'no such directory' => 'file://' . (DIRECTORY_SEPARATOR === '\\' ? '/' : '')
		. str_replace('\\', '/', $dir) . '/nodir/x.xml',
	'a drive-like scheme' => 'zz:/phl-xw-nodir/x.xml',
	'relative, no such directory' => 'phl-xw-nodir/x.xml',
	'a directory' => $dir,
	'a scheme no wrapper takes' => 'nosuchscheme://x',
] as $why => $uri) {
	xw_c5_try("toUri: $why", fn() => get_class(XMLWriter::toUri($uri)));
	xw_c5_try("openUri: $why", function () use ($uri) { $w = new XMLWriter; return @$w->openUri($uri); });
}
restore_error_handler();

xw_c5_try('startDocument nul encoding', function () {
	$x = new XMLWriter;
	$x->openMemory();
	return $x->startDocument('1.0', "UT\0F-8");
});
xw_c5_try('startDocument nul version', function () {
	$x = new XMLWriter;
	$x->openMemory();
	$r = $x->startDocument("1.\0 0");
	return [$r, $x->outputMemory()];
});

foreach (glob($dir . '/*') as $f) { @unlink($f); }
@rmdir($dir);
--EXPECT--
true
<?xml version="1.0" encoding="UTF-8"?>
<urlset>
 <loc>https://example.test/</loc>
</urlset>
before release: '' / outputMemory ''
after release: '<a>1</a>'
toMemory XMLWriter <a>1</a>
toUri XMLWriter <b>2</b>
toStream XMLWriter <c>3</c>
subclass XwC5Kid <d>4</d>
static through an instance: XMLWriter
php://memory <e>5</e>
openUri empty: ValueError: XMLWriter::openUri(): Argument #1 ($uri) must not be empty
openUri nul: ValueError: XMLWriter::openUri(): Argument #1 ($uri) must not contain any null bytes
toUri nul: ValueError: XMLWriter::toUri(): Argument #1 ($uri) must not contain any null bytes
toStream int: TypeError: XMLWriter::toStream(): Argument #1 ($stream) must be of type resource, int given
toStream closed: TypeError: XMLWriter::toStream(): supplied resource is not a valid stream resource
toUri: no such directory: ValueError: XMLWriter::toUri(): Argument #1 ($uri) must resolve to a valid file path
openUri: no such directory: false
toUri: a drive-like scheme: Error: Could not construct libxml writer
openUri: a drive-like scheme: false
toUri: relative, no such directory: ValueError: XMLWriter::toUri(): Argument #1 ($uri) must resolve to a valid file path
openUri: relative, no such directory: false
toUri: a directory: Error: Could not construct libxml writer
openUri: a directory: false
toUri: a scheme no wrapper takes: Error: Could not construct libxml writer
openUri: a scheme no wrapper takes: false
startDocument nul encoding: ValueError: XMLWriter::startDocument(): Argument #2 ($encoding) must not contain any null bytes
startDocument nul version: array (
  0 => true,
  1 => '<?xml version="1."?>
',
)
