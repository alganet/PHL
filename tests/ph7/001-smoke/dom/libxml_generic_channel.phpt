--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
libxml GENERIC-channel diagnostics are captured, not printed past the engine
--FILE--
<?php
// libxml reports an unbound FUNCTION prefix through its GENERIC channel on some
// library versions and its structured one on others, with different wording and
// code each way -- so this pins only what does NOT depend on the version: the
// diagnostic is CAPTURED, never printed past the engine, and is reported under
// the calling method's name when capture is off.
$d = new DOMDocument;
$d->loadXML('<r><a/></r>');
$xp = new DOMXPath($d);
$prev = libxml_use_internal_errors(true);
var_dump($xp->evaluate('zz:nope()'));
$errs = libxml_get_errors();
var_dump(count($errs), $errs[0]->level, strlen($errs[0]->message) > 0, $errs[0]->line, $errs[0]->column, $errs[0]->file);
$last = libxml_get_last_error();
var_dump($last->message === $errs[0]->message, $last->code === $errs[0]->code);
libxml_use_internal_errors($prev);
libxml_clear_errors();
// capture off: a php diagnostic under the method's own name, and NOTHING on stdout
set_error_handler(function ($n, $s) { echo "ERR[$n] ", (strncmp($s, 'DOMXPath::evaluate(): ', 22) === 0 ? 'evaluate-prefixed' : $s), "\n"; return true; });
var_dump($xp->evaluate('zz:nope()'));
restore_error_handler();
set_error_handler(function ($n, $s) { echo "ERR[$n] ", (strncmp($s, 'DOMXPath::query(): ', 19) === 0 ? 'query-prefixed' : $s), "\n"; return true; });
var_dump($xp->query('zz:nope()'));
restore_error_handler();
--EXPECT--
bool(false)
int(1)
int(2)
bool(true)
int(0)
int(0)
string(0) ""
bool(true)
bool(true)
ERR[2] evaluate-prefixed
bool(false)
ERR[2] query-prefixed
bool(false)
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
