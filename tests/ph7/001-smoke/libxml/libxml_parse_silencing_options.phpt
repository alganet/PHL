--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
LIBXML_NOERROR and LIBXML_NOWARNING silence a parse's diagnostics, not its record
--FILE--
<?php
// Reset shared state (the in-process smoke runner shares one interpreter)
libxml_use_internal_errors(false);
libxml_clear_errors();
set_error_handler(function ($no, $str) { echo "  diag $no: ", str_replace("\n", '', $str), "\n"; return true; });

// One document per severity: a DTD the content does not follow is libxml's
// ERROR, an unsupported version is its WARNING.
$err  = '<!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>';
$warn = '<?xml version="1.1"?><r/>';
foreach ([0, LIBXML_NOERROR, LIBXML_NOWARNING, LIBXML_NOERROR | LIBXML_NOWARNING] as $o) {
    echo "options=$o\n";
    $d = new DOMDocument;
    $d->validateOnParse = true;
    $d->loadXML($err, $o);
    $w = new DOMDocument;
    $w->loadXML($warn, $o);
}

// What is silenced is the PRINT. libxml_get_last_error() still answers, and so
// does the queue when internal errors are on.
libxml_clear_errors();
$d = new DOMDocument;
$d->validateOnParse = true;
$d->loadXML($err, LIBXML_NOERROR);
$e = libxml_get_last_error();
echo 'last=', $e === false ? 'none' : $e->level . ' ' . $e->code . ' ' . trim($e->message), "\n";
libxml_clear_errors();

libxml_use_internal_errors(true);
$q = new DOMDocument;
$q->validateOnParse = true;
$q->loadXML($err, LIBXML_NOERROR | LIBXML_NOWARNING);
foreach (libxml_get_errors() as $x) {
    echo 'queued ', $x->level, ' ', $x->code, ' ', trim($x->message), "\n";
}
libxml_clear_errors();
libxml_use_internal_errors(false);

// The option reaches load() too, and a message it silences never reaches the
// line buffer either: the held fragment of the FIRST parse below is dropped
// with it, so the second parse prints its own diagnostic alone.
$file = rtrim(sys_get_temp_dir(), '/\\') . '/phl_libxml_silence.xml';
file_put_contents($file, $err);
$f = new DOMDocument;
$f->validateOnParse = true;
echo 'load silenced=', var_export($f->load($file, LIBXML_NOERROR), true), "\n";
$g = new DOMDocument;
$g->validateOnParse = true;
$g->loadXML('<r/>');
$h = new DOMDocument;
$h->loadXML($warn);
// The cleanup goes OUTSIDE the handler: this test's diagnostics are its
// subject, so its handler cannot screen on error_reporting() (the in-process
// corpus does not promise an ambient value), and an `@` under a handler that
// records everything prints whatever the unlink happens to say.
restore_error_handler();
@unlink($file);
--EXPECT--
options=0
  diag 2: DOMDocument::loadXML(): No declaration for element b in Entity, line: 1
  diag 2: DOMDocument::loadXML(): Element r content does not follow the DTD, expecting (a), got (b) in Entity, line: 1
  diag 8: DOMDocument::loadXML(): Unsupported version '1.1' in Entity, line: 1
options=32
  diag 8: DOMDocument::loadXML(): Unsupported version '1.1' in Entity, line: 1
options=64
  diag 2: DOMDocument::loadXML(): No declaration for element b in Entity, line: 1
  diag 2: DOMDocument::loadXML(): Element r content does not follow the DTD, expecting (a), got (b) in Entity, line: 1
options=96
last=2 504 Element r content does not follow the DTD, expecting (a), got (b)
queued 2 534 No declaration for element b
queued 2 504 Element r content does not follow the DTD, expecting (a), got (b)
load silenced=true
  diag 8: DOMDocument::loadXML(): Validation failed: no DTD found !Unsupported version '1.1' in Entity, line: 1
