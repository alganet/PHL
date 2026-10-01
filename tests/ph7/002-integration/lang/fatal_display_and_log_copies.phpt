--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A fatal prints php's TWO copies: `Fatal error: ` on stdout and `PHP Fatal error:  ` on stderr
--DESCRIPTION--
php gives a warning or a notice two copies behind two separate ini gates -- a
log copy on stderr under log_errors and a display copy on stdout under
display_errors -- and they are NOT the same bytes: the display one has no `PHP `
prefix, one space after the colon and a leading blank line. Its fatals follow the
same rule, and this engine's did not: every fatal was built in the log shape and
then written to whichever stream happened to be enabled, so display_errors=1 put
the stderr wording on stdout and both-on printed one copy where php prints two.
Stock CLI (display_errors off, log_errors on) is the single setting where that
routing came out right, which is why it stood for so long.

The `$previous` chain is deliberate: the label belongs to the HEAD entry alone,
and each further entry appends `Next ...` into the same body.

XDEBUG_MODE=off because the oracle on the development box carries xdebug, whose
develop mode rewrites the `Stack trace:` block this test pins.
--ENV--
XDEBUG_MODE=off
--INI--
display_errors=1
log_errors=1
--FILE--
<?php
echo "before\n";
try {
    throw new RuntimeException("inner");
} catch (\Throwable $e) {
    throw new LogicException("outer", 0, $e);
}
--EXPECTF--
before

Fatal error: Uncaught RuntimeException: inner in %s:4
Stack trace:
#0 {main}

Next LogicException: outer in %s:6
Stack trace:
#0 {main}
  thrown in %s on line 6
--EXPECT_STDERR--
PHP Fatal error:  Uncaught RuntimeException: inner in %s:4
Stack trace:
#0 {main}

Next LogicException: outer in %s:6
Stack trace:
#0 {main}
  thrown in %s on line 6
--CLEAN--
<?php
