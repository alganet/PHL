--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
null parameter type accepts null and rejects a non-null arg without coercion (PHP 8.2)
--FILE--
<?php
function npTakesNull(null $x) { return $x; }
echo npTakesNull(null) === null ? "null_ok\n" : "null_fail\n";
// A non-null arg must be a TypeError, NOT silently coerced to null.
try { npTakesNull(5); echo "not_rejected\n"; }
catch (TypeError $e) { echo "int_rejected\n"; }
?>
--EXPECT--
null_ok
int_rejected
--CLEAN--
<?php
