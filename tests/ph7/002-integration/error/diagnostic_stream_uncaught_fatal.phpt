--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An uncaught exception's fatal report goes to stderr, leaving stdout clean
--DESCRIPTION--
The `PHP Fatal error:  Uncaught ...` report + stack trace follows the same gates
as warnings: under the stock gates it is written to STDERR only, so program
output already emitted to STDOUT is not intermixed with the fatal.
--INI--
display_errors=0
log_errors=1
--FILE--
<?php
echo "before\n";
throw new RuntimeException("boom");
?>
--EXPECT--
before
--EXPECT_STDERR--
PHP Fatal error:  Uncaught RuntimeException: boom in %s:%d
Stack trace:
#0 {main}
  thrown in %s on line %d
