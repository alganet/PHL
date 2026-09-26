--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMDocument loadHTML/loadHTMLFile/saveHTML/saveHTMLFile
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
$dir = rtrim(sys_get_temp_dir(), '/\\') . '/phl_dom_html';
@mkdir($dir);
$hide = function ($s) use (&$dir) {
    $s = str_replace(['\\', "\n"], ['/', '\\n'], (string)$s);
    return str_ireplace(str_replace('\\', '/', $dir), 'DIR', $s);
};
set_error_handler(function ($no, $str) use ($hide) { echo "diag $no: ", $hide($str), "\n"; return true; });

// The HTML parser closes what the markup left open and supplies the implied
// elements, and what comes out is an HTML DOCUMENT (node type 13) with no URI.
$d = new DOMDocument;
var_dump($d->loadHTML('<p>unclosed'));
echo 'type=', $d->nodeType, ' name=', $d->nodeName, ' uri=', var_export($d->documentURI, true),
     ' version=', var_export($d->version, true), ' standalone=', var_export($d->standalone, true), "\n";
echo $hide($d->saveHTML()), "\n";
// The XML serializer still writes XML for it: a declaration, and `<br/>` where
// the HTML one writes `<br>`.
$v = new DOMDocument;
$v->loadHTML('<p>a</p><br>');
echo 'html=', $hide($v->saveHTML()), "\n";
echo 'xml =', $hide($v->saveXML()), "\n";

// The two HTML options: one drops the implied html/body, the other the DTD.
foreach ([0, LIBXML_HTML_NOIMPLIED, LIBXML_HTML_NODEFDTD,
          LIBXML_HTML_NOIMPLIED | LIBXML_HTML_NODEFDTD] as $o) {
    $p = new DOMDocument;
    $p->loadHTML('<p>x</p>', $o);
    printf("options=%-5d %s\n", $o, $hide($p->saveHTML()));
}

// A node dump is the node's own subtree, and it reads formatOutput.
$n = new DOMDocument;
$n->loadHTML('<html><body><div><p>x</p><p>y</p></div></body></html>');
$div = $n->getElementsByTagName('div')->item(0);
echo 'node   =', $hide($n->saveHTML($div)), "\n";
$n->formatOutput = true;
echo 'node fmt=', $hide($n->saveHTML($div)), "\n";
$n->formatOutput = false;
// ...a node of ANOTHER document is the Wrong Document refusal
$other = new DOMDocument;
$other->loadXML('<r/>');
try { $n->saveHTML($other->documentElement); }
catch (\Throwable $e) { echo get_class($e), '(', $e->getCode(), '): ', $e->getMessage(), "\n"; }
$n->strictErrorChecking = false;
var_dump($n->saveHTML($other->documentElement));

// The file pair. Writing to a file STAMPS the document with the encoding meta
// libxml is about to use -- in the document, so the next saveHTML shows it.
$f = "$dir/page.html";
$w = new DOMDocument;
$w->loadHTML('<!DOCTYPE html><html><head><title>T</title></head><body>x</body></html>');
// (the SPELLING of that meta is libxml's version's answer -- an http-equiv
// Content-Type on 2.9, a charset attribute on 2.13 -- so what is pinned here is
// that one appeared, that it names UTF-8, and that the file is what the
// document now says. PLAN 7.4.)
echo 'before metas=', $w->getElementsByTagName('meta')->length, "\n";
$n = $w->saveHTMLFile($f);
echo 'after metas=', $w->getElementsByTagName('meta')->length,
     ' utf8=', var_export(str_contains(strtoupper($w->saveHTML()), 'UTF-8'), true), "\n";
echo 'bytes match=', var_export($n === strlen(file_get_contents($f)), true),
     ' file is the document=', var_export(file_get_contents($f) === $w->saveHTML(), true), "\n";

$r = new DOMDocument;
var_dump($r->loadHTMLFile($f));
echo 'uri names the file=', var_export(str_ends_with($r->documentURI, 'page.html'), true),
     ' title=', $r->getElementsByTagName('title')->item(0)->textContent, "\n";

// Refusals: the empty name/source from all four, and a file that is not there.
$miss = new DOMDocument;
var_dump($miss->loadHTMLFile("$dir/nope.html"));
foreach (['loadHTML', 'loadHTMLFile', 'saveHTMLFile'] as $m) {
    try { $miss->$m(''); }
    catch (\Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
// ...and a destination that cannot be opened answers the bytes it wrote: none.
var_dump($w->saveHTMLFile("$dir/no-such-dir/x.html"));

array_map('unlink', glob("$dir/*"));
@rmdir($dir);
--EXPECT--
bool(true)
type=13 name=#document uri=NULL version=NULL standalone=true
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN" "http://www.w3.org/TR/REC-html40/loose.dtd">\n<html><body><p>unclosed</p></body></html>\n
html=<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN" "http://www.w3.org/TR/REC-html40/loose.dtd">\n<html><body><p>a</p><br></body></html>\n
xml =<?xml version="1.0" standalone="yes"?>\n<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN" "http://www.w3.org/TR/REC-html40/loose.dtd">\n<html><body><p>a</p><br/></body></html>\n
options=0     <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN" "http://www.w3.org/TR/REC-html40/loose.dtd">\n<html><body><p>x</p></body></html>\n
options=8192  <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN" "http://www.w3.org/TR/REC-html40/loose.dtd">\n<p>x</p>\n
options=4     <html><body><p>x</p></body></html>\n
options=8196  <p>x</p>\n
node   =<div><p>x</p><p>y</p></div>
node fmt=<div>\n<p>x</p>\n<p>y</p>\n</div>
DOMException(4): Wrong Document Error
diag 2: DOMDocument::saveHTML(): Wrong Document Error
bool(false)
before metas=0
after metas=1 utf8=true
bytes match=true file is the document=true
bool(true)
uri names the file=true title=T
diag 2: DOMDocument::loadHTMLFile(): I/O warning : failed to load external entity "DIR/nope.html"
bool(false)
ValueError: DOMDocument::loadHTML(): Argument #1 ($source) must not be empty
ValueError: DOMDocument::loadHTMLFile(): Argument #1 ($filename) must not be empty
ValueError: DOMDocument::saveHTMLFile(): Argument #1 ($filename) must not be empty
diag 2: DOMDocument::saveHTMLFile(DIR/no-such-dir/x.html): Failed to open stream: No such file or directory
int(0)
