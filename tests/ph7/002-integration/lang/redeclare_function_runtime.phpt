--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A function redeclared at RUN time is php's fatal, with php's label and stack trace
--DESCRIPTION--
XDEBUG_MODE=off because the oracle on the development box carries xdebug, whose
develop mode REPLACES php's `Stack trace:` block with its own numbering -- and
this test exists to pin exactly that block. Every other fatal test in this corpus
absorbs the difference with a `%A`, which is why nobody noticed the trace was
missing here for as long as it was.
--ENV--
XDEBUG_MODE=off
--FILE--
<?php
/* A conditional/nested declaration is bound when its statement RUNS, so the name
 * is taken on the first call and colliding with it on the second is php's fatal
 * -- the same declaration naming its own line as the previous one. This engine
 * used to make the second run a silent no-op, and reported the collisions it did
 * catch through the ordinary diagnostic printer: a `PHP Error:  ` label, a
 * message with neither `function` nor the `(previously declared in ...)` clause,
 * and no stack trace at all. */
function redecl_rt_mk() { function redecl_rt_h() {} }
echo "pre\n";
redecl_rt_mk();
redecl_rt_mk();
echo "unreached";
--EXPECTF--
pre
%AFatal error:  Cannot redeclare function redecl_rt_h() (previously declared in %s:9) in %s on line 9
Stack trace:
#0 %s(12): redecl_rt_mk()
#1 {main}%A
--CLEAN--
<?php
