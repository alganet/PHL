--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
microtime() argument handling
--DESCRIPTION--
php enforces a MAXIMUM argument count; microtime(true, "extra") is an ArgumentCountError,
not a silently-ignored extra. A non-bool first argument still coerces. Asserted the
permissive behavior from behind a bare skip before.
--FILE--
<?php
$result1 = microtime("invalid");
echo is_float($result1) ? "FLOAT_OK\n" : "FLOAT_FAIL\n";
try { microtime(true, "extra"); } catch (\ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
echo is_float(microtime(true)) ? "FLOAT_OK\n" : "FLOAT_FAIL\n";
?>
--EXPECT--
FLOAT_OK
microtime() expects at most 1 argument, 2 given
FLOAT_OK
