--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
display_errors=stderr routes an uncaught fatal's display copy, stack trace and all
--DESCRIPTION--
A fatal report takes the same destination a warning does, and the whole body --
the sentence, the location and the `Stack trace:` block -- travels with it.
--INI--
display_errors=stderr
log_errors=0
--FILE--
<?php
echo "OUT\n";
throw new RuntimeException("boom");
?>
--EXPECT--
OUT
--EXPECT_STDERR--
Fatal error: Uncaught RuntimeException: boom in %s:%d
Stack trace:
#0 {main}
  thrown in %s on line %d
