--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`use ($GLOBALS)` is php's compile fatal, not an accepted capture
--DESCRIPTION--
php rejects importing an auto-global into a closure's lexical scope. PHL accepted it and then
overwrote the superglobal's slot with the imported snapshot, taking the live symbol table with
it (see closure_never_captures_an_auto_global.phpt). It is the same fatal in both engines now;
PHL's native fatal omits php's `Stack trace:` tail, which is the standing §6 shape divergence,
and the two engines prefix the line differently (log copy vs display copy), so the assertion is
on the message BODY and its line number.
--FILE--
<?php
$g = function () use ($GLOBALS) { return 1; };
echo "NOT REACHED\n";
?>
--EXPECTF--
%AFatal error:%ACannot use auto-global as lexical variable in %s on line 2%A
