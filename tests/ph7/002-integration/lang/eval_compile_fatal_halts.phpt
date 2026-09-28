--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
eval()'s parse error is catchable; its compile FATAL is not
--DESCRIPTION--
The same split the include path makes (ECOSYSTEM.md F30), through the other door.
php's eval() throws a ParseError the caller may catch, and raises a refusal of
E_ERROR severity as an uncatchable fatal that ends the process -- where this engine
made BOTH a catchable ParseError, so a program went on running past a declaration
php had already stopped for.
--SKIPIF--
<?php
// php names an eval'd unit `<file>(<line>) : eval()'d code` in the diagnostic and
// prints a `Stack trace:` under it; this engine names the outer file and prints no
// trace (ECOSYSTEM.md F30, behind F6). The VERDICT is what is pinned here.
if (function_exists('zend_version')) { echo "skip php names the eval'd unit and prints a Stack trace"; }
?>
--FILE--
<?php
echo "before\n";
try { eval('$a = ;'); } catch (ParseError $e) { echo 'caught: ', $e->getMessage(), "\n"; }
try { eval('class EvFatalDup {} class EvFatalDup {}'); }
catch (Throwable $e) { echo "unreached: caught ", get_class($e), "\n"; }
echo "unreached\n";
?>
--EXPECTF--
before
caught: syntax error, unexpected token ";"
%AFatal error:  Cannot redeclare class EvFatalDup%A
