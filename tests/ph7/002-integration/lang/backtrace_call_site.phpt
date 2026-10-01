--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A backtrace frame names the file and line the CALL is written in
--DESCRIPTION--
A frame's file used to be derived when the trace was TAKEN, by reading the
caller's function through the parent frame -- which is a `try` block's own
function-less frame for a call written inside one, so it fell back to the include
stack's top and blamed the ENTRY script. The same fallback blamed a call made by an
included file's top-level code on whatever happened to be being included at the
moment of the trace. And a call spanning several lines was recorded at its LAST one,
where php records its first: the emitter stamps an instruction with the token the
generator stands on, which for a call is the closing parenthesis.

Where it bit: PHPUnit filters its own frames out of a failure trace by file path, so
seven internal frames survived the filter and were printed under every failure.
--FILE--
<?php
require __DIR__ . '/backtrace_call_site.inc';
$t = new BcsT;
foreach (['plain','inTry','inCatch','inFinally','nested','multi','tryMulti'] as $m) { $t->$m(); }
bcsGlobal();
try { bcsWhere('mainTry'); } catch (Throwable $e) {}
bcsWhere(
    'mainMulti'
);
?>
--EXPECT--
plain       backtrace_call_site.inc:9
inTry       backtrace_call_site.inc:10
inCatch     backtrace_call_site.inc:11
inFinally   backtrace_call_site.inc:12
nested      backtrace_call_site.inc:13
multi       backtrace_call_site.inc:14
tryMulti    backtrace_call_site.inc:17
global      backtrace_call_site.inc:21
mainTry     backtrace_call_site.phpt.file:6
mainMulti   backtrace_call_site.phpt.file:7
