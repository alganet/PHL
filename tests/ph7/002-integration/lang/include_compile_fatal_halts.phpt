--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A compile-time FATAL in an included file is uncatchable and stops the program
--DESCRIPTION--
php splits a failed compile in two: the parser's refusal is a catchable ParseError
(pinned next door), and a refusal of E_ERROR severity is an uncatchable fatal that
ends the process -- a `catch (Throwable)` around the require does not see it. This
engine printed both and then carried on.
--SKIPIF--
<?php
// Both engines stop here and word the refusal identically; php prints a
// `Stack trace:` block under a compile-time FATAL that this engine does not
// (it needs the frame attribution first).
if (function_exists('zend_version')) { echo 'skip php prints a Stack trace under a compile-time fatal'; }
?>
--FILE--
<?php
echo "before\n";
try {
    require __DIR__ . '/include_compile_fatal.inc';
} catch (Throwable $e) {
    echo "unreached: caught ", get_class($e), "\n";
}
echo "unreached\n";
?>
--EXPECTF--
before
%AFatal error:  Cannot redeclare class IncFatalDup%A
