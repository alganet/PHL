--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Uncaught report for a finally-supersede chains the original try exception as "Next"
--FILE--
<?php
try { throw new Exception("A from try"); }
finally { throw new Exception("B from finally"); }
?>
--EXPECTF--
%s Fatal error:  Uncaught Exception: A from try in %s
Stack trace:
%A
Next Exception: B from finally in %s
Stack trace:
%A
  thrown in %s on line %d
--CLEAN--
<?php
