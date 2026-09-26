--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
libxml GENERIC-channel diagnostics are captured, not printed past the engine
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r><a/></r>');
$xp = new DOMXPath($d);
// libxml's GENERIC channel: an unbound prefix on a FUNCTION name is reported there
$prev = libxml_use_internal_errors(true);
var_dump($xp->evaluate('zz:nope()'));
foreach (libxml_get_errors() as $e) {
  printf("level=%d code=%d line=%d col=%d file=%s msg=%s\n",
    $e->level, $e->code, $e->line, $e->column, var_export($e->file, true), var_export($e->message, true));
}
$last = libxml_get_last_error();
var_dump($last->code, $last->message);
libxml_use_internal_errors($prev);
libxml_clear_errors();
// ...and with capture off it is a php diagnostic under the method's own name
set_error_handler(function ($n, $s) { echo "ERR[$n]: ", $s, "\n"; return true; });
var_dump($xp->evaluate('zz:nope()'));
var_dump($xp->query('zz:nope()'));
restore_error_handler();
--EXPECT--
bool(false)
level=2 code=1 line=0 col=0 file='' msg='xmlXPathCompOpEval: function nope bound to undefined prefix zz'
int(1)
string(62) "xmlXPathCompOpEval: function nope bound to undefined prefix zz"
ERR[2]: DOMXPath::evaluate(): xmlXPathCompOpEval: function nope bound to undefined prefix zz
bool(false)
ERR[2]: DOMXPath::query(): xmlXPathCompOpEval: function nope bound to undefined prefix zz
bool(false)
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
