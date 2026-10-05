--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An output handler that throws during the shutdown flush is fatal, and no open buffer is sent
--INI--
display_errors=stderr
log_errors=0
--FILE--
<?php
// The shutdown flush has no frame left to throw into: php's throw is fatal at
// once and the request bails out, so neither this buffer nor the one below is
// sent -- not the input, not what the handler printed.
echo "before\n";
function upper($b) { return strtoupper($b); }
function throws($b) { echo "<printed>"; throw new Exception("from handler"); }
ob_start('upper');
echo "outer ";
ob_start('throws');
echo "inner";
--EXPECT--
before
--EXPECT_STDERR--
Fatal error: Uncaught Exception: from handler in %s:%d
Stack trace:
#0 [internal function]: throws()
%A
  thrown in %s on line %d
