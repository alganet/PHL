--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMDocument::load()/save(): a document read from and written to a file
--SKIPIF--
<?php
// php's Windows build ships libxml 2.11, which spells a local path as a file:/
// URI (documentURI and every file diagnostic) and cannot load a backslashed
// absolute DTD path; PHL answers as php does on Linux and macOS. Only the
// oracle is skipped there -- PHL runs this test on every platform.
if (function_exists('zend_version') && PHP_OS_FAMILY === 'Windows') {
    echo 'skip the Windows oracle libxml spells local paths as file: URIs';
}
?>
--FILE--
<?php
$dir = rtrim(sys_get_temp_dir(), '/\\') . '/phl_dom_io';
// Every path printed below is the temp directory's, which is the machine's:
// the expectation names it DIR.
// (the win32 spelling of the same directory is the same directory: paths are
// normalised to one separator, and matched without case, before the name is
// taken out)
$hide = function ($s) use (&$dir) {
    $s = str_replace(['\\', "\n"], ['/', ''], (string)$s);
    // macOS's temp dir sits behind a symlink (/var -> /private/var) and a message
    // may name either spelling, so both become DIR -- the resolved one first.
    $names = [str_replace('\\', '/', $dir)];
    if (($real = realpath($dir)) !== false) {
        array_unshift($names, str_replace('\\', '/', $real));
    }
    return str_ireplace($names, 'DIR', $s);
};
// The scaffolding runs BEFORE the handler is installed: an `@`-suppressed call
// under a handler that records everything prints whatever it happens to say, and
// a directory left behind by an interrupted run is enough to make it say
// something (that is what turned session_cookie_params red on the Windows gate).
// Screening on error_reporting() would work in a subprocess, but the smoke
// corpus shares ONE interpreter and does not promise an ambient value.
@mkdir($dir);
set_error_handler(function ($no, $str) use ($hide) { echo "diag $no: ", $hide($str), "\n"; return true; });
$in = "$dir/in.xml";
file_put_contents($in, "<?xml version=\"1.0\" encoding=\"US-ASCII\"?>\n<r a=\"1\">\n  <x>t</x>\n  <e/>\n</r>\n");

// The parse is loadXML's, from a file: the declaration comes back the same way
// and the document's URI is the file it was read from.
$d = new DOMDocument;
var_dump($d->load($in));
// The document's URI is the file it came from -- as a URI, so the separators
// and any character that has to be escaped are libxml's spelling of the path.
echo 'uri names the file=', var_export(str_ends_with($d->documentURI, 'in.xml'), true),
     ' base==uri=', var_export($d->documentElement->baseURI === $d->documentURI, true), "\n";
echo 'version=', $d->version, ' encoding=', $d->encoding,
     ' kids=', $d->documentElement->childNodes->length, "\n";
// ...and the directives reach it, as they reach loadXML
$w = new DOMDocument;
$w->preserveWhiteSpace = false;
$w->load($in);
echo 'noblanks kids=', $w->documentElement->childNodes->length, "\n";

// save() answers the BYTE COUNT, and writes what saveXML() would have answered.
$out = "$dir/out.xml";
$n = $d->save($out);
echo 'saved=', var_export($n, true), ' equal=', var_export(file_get_contents($out) === $d->saveXML(), true), "\n";
$d->formatOutput = true;
echo 'formatted=', var_export($d->save($out), true), "\n";

// The two save OPTIONS php reads. `LIBXML_NOEMPTYTAG` reaches both savers (it
// is a library-wide switch, so a NODE dump obeys it too); `LIBXML_NOXMLDECL` is
// saveXML's alone -- a saved FILE always carries its declaration.
$p = new DOMDocument;
$p->loadXML('<r><e/><x>t</x></r>');
foreach ([0, LIBXML_NOEMPTYTAG, LIBXML_NOXMLDECL, LIBXML_NOEMPTYTAG | LIBXML_NOXMLDECL] as $o) {
    printf("opt=%-2d whole=%-46s node=%s\n", $o,
        str_replace("\n", '', $p->saveXML(null, $o)),
        str_replace("\n", '', $p->saveXML($p->documentElement, $o)));
    $p->save($out, $o);
    printf("       file=%s\n", str_replace("\n", '', file_get_contents($out)));
}

// Refusals. A file that is not there is libxml's own wording; the empty name
// and a name with a NUL byte are php's ValueErrors, from both methods.
$miss = new DOMDocument;
$miss->loadXML('<keep/>');
var_dump($miss->load("$dir/nope.xml"));
echo 'kept=', str_replace("\n", '', $miss->saveXML()), "\n";
foreach (['load', 'save'] as $m) {
    foreach (['', "$dir/a\0b"] as $bad) {
        try { $miss->$m($bad); }
        catch (\Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    }
}
var_dump($miss->save("$dir/no-such-dir/x.xml"));

// An empty file is not a document.
$empty = "$dir/empty.xml";
file_put_contents($empty, '');
$e = new DOMDocument;
var_dump($e->load($empty));

// The queue keeps libxml's own copy of an I/O failure, and it is the WARNING
// level with no file and no line -- while what gets printed above is php's
// E_WARNING with the severity spelled into the text.
libxml_use_internal_errors(true);
$q = new DOMDocument;
var_dump($q->load("$dir/nope.xml"));
foreach (libxml_get_errors() as $err) {
    echo 'queued level=', $err->level, ' code=', $err->code, ' line=', $err->line,
         ' file=', var_export($err->file, true), ' msg=', $hide(trim($err->message)), "\n";
}
libxml_clear_errors();
libxml_use_internal_errors(false);

array_map('unlink', glob("$dir/*"));
restore_error_handler();
@rmdir($dir);
--EXPECT--
bool(true)
uri names the file=true base==uri=true
version=1.0 encoding=US-ASCII kids=5
noblanks kids=2
saved=75 equal=true
formatted=75
opt=0  whole=<?xml version="1.0"?><r><e/><x>t</x></r>       node=<r><e/><x>t</x></r>
       file=<?xml version="1.0"?><r><e/><x>t</x></r>
opt=4  whole=<?xml version="1.0"?><r><e></e><x>t</x></r>    node=<r><e></e><x>t</x></r>
       file=<?xml version="1.0"?><r><e></e><x>t</x></r>
opt=2  whole=<r><e/><x>t</x></r>                            node=<r><e/><x>t</x></r>
       file=<?xml version="1.0"?><r><e/><x>t</x></r>
opt=6  whole=<r><e></e><x>t</x></r>                         node=<r><e></e><x>t</x></r>
       file=<?xml version="1.0"?><r><e></e><x>t</x></r>
diag 2: DOMDocument::load(): I/O warning : failed to load external entity "DIR/nope.xml"
bool(false)
kept=<?xml version="1.0"?><keep/>
ValueError: DOMDocument::load(): Argument #1 ($filename) must not be empty
ValueError: DOMDocument::load(): Argument #1 ($filename) must not contain any null bytes
ValueError: DOMDocument::save(): Argument #1 ($filename) must not be empty
ValueError: DOMDocument::save(): Argument #1 ($filename) must not contain any null bytes
diag 2: DOMDocument::save(DIR/no-such-dir/x.xml): Failed to open stream: No such file or directory
bool(false)
diag 2: DOMDocument::load(): Document is empty in DIR/empty.xml, line: 1
bool(false)
bool(false)
queued level=1 code=1549 line=0 file='' msg=failed to load external entity "DIR/nope.xml"
