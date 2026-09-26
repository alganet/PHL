--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
libxml WARNINGs surface as E_NOTICE and ERRORs as E_WARNING
--FILE--
<?php
// Reset shared state (the in-process smoke runner shares one interpreter)
libxml_use_internal_errors(false);
libxml_clear_errors();

$seen = [];
set_error_handler(function ($no, $msg) use (&$seen) {
    $seen[] = $no . ' ' . $msg;
    return true;
});

// libxml calls this one a WARNING (level 1): php reports E_NOTICE (8).
$warn = new DOMDocument;
var_dump($warn->loadXML('<?xml version="1.1"?><r/>'));

// ...and this one an ERROR (level 3 fatal): php reports E_WARNING (2).
$err = new DOMDocument;
var_dump($err->loadXML('<r>'));

restore_error_handler();
foreach ($seen as $line) {
    echo $line, "\n";
}

// The QUEUE keeps libxml's own level either way.
libxml_use_internal_errors(true);
$q = new DOMDocument;
$q->loadXML('<?xml version="1.1"?><r/>');
foreach (libxml_get_errors() as $e) {
    echo 'queued level=', $e->level, ' line=', $e->line, "\n";
}
libxml_clear_errors();
libxml_use_internal_errors(false);
--EXPECT--
bool(true)
bool(false)
8 DOMDocument::loadXML(): Unsupported version '1.1' in Entity, line: 1
2 DOMDocument::loadXML(): Premature end of data in tag r line 1 in Entity, line: 1
queued level=1 line=1
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
