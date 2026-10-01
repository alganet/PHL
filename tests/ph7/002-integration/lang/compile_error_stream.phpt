--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A compile-time diagnostic goes to STDERR, and program STDOUT stays empty
--DESCRIPTION--
php's log copy of a compile diagnostic goes to stderr under log_errors, exactly
as a runtime one does; stock CLI has display_errors off, so nothing reaches
stdout at all. The engine used to hand every compile diagnostic to the
engine-level compile-error consumer, which this CLI pointed at STDOUT -- so
`phl -l bad.php` and `phl bad.php` both wrote php's stderr text into program
output, where anything capturing a script's stdout reads it as data. That is the
stream a composer/CI toolchain parses.

XDEBUG_MODE=off keeps the oracle's own module out of the comparison.
--ENV--
XDEBUG_MODE=off
--FILE--
<?php
$a = ;
--EXPECT--
--EXPECT_STDERR--
PHP Parse error:  syntax error, unexpected token ";" in %s on line 2
--CLEAN--
<?php
