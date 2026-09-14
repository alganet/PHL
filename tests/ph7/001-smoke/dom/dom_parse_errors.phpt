--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOM parse failures populate the libxml error queue (PHPUnit Loader pattern)
--FILE--
<?php
libxml_use_internal_errors(true);
$d = new DOMDocument;
var_dump($d->loadXML('<test>'));
foreach (libxml_get_errors() as $e) {
    printf("level=%d code=%d line=%d msg=%s\n", $e->level, $e->code, $e->line, trim($e->message));
}
libxml_clear_errors();
$d->loadXML("<a>\n<b>\n</wrong>\n</a>");
foreach (libxml_get_errors() as $e) {
    printf("level=%d code=%d line=%d msg=%s\n", $e->level, $e->code, $e->line, trim($e->message));
}
$last = libxml_get_last_error();
var_dump($last->line > 0, $last instanceof LibXMLError);
libxml_use_internal_errors(false);
// The PHPUnit Util\Xml\Loader accumulation pattern
$document = new DOMDocument;
$document->preserveWhiteSpace = false;
$internal = libxml_use_internal_errors(true);
$message = '';
$reporting = error_reporting(0);
$loaded = $document->loadXML('<test>');
foreach (libxml_get_errors() as $error) {
    $message .= "\n" . $error->message;
}
libxml_use_internal_errors($internal);
error_reporting($reporting);
var_dump($loaded);
var_export($message); echo "\n";
var_dump((bool)preg_match("#Premature end of data in tag test line 1|EndTag: '</' not found#", $message));
--EXPECT--
bool(false)
level=3 code=77 line=1 msg=Premature end of data in tag test line 1
level=3 code=76 line=3 msg=Opening and ending tag mismatch: b line 2 and wrong
bool(true)
bool(true)
bool(false)
'
Premature end of data in tag test line 1
'
bool(true)
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
