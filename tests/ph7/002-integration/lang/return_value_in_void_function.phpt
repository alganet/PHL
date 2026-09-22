--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
returning a value from a void function is a COMPILE error, not a runtime TypeError
--FILE--
<?php
// Nothing here runs: php stops at the return statement below.
echo "unreachable\n";
function bad(): void { return 1; }
?>
--EXPECTF--
%s Fatal error:  A void function must not return a value in %s
--CLEAN--
<?php
