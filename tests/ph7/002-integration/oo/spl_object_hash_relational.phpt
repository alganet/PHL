--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_object_hash() returns a 32-char string, stable per object and distinct across objects
--DESCRIPTION--
The exact hash value is PHP-internal-handle derived and NOT reproducible; PHL
returns the zero-padded object id. Only the guaranteed properties (length 32,
stable per object, distinct objects -> distinct) are asserted, so this runs
under both engines.
--FILE--
<?php
class C {}
$a = new C; $b = new C;
echo strlen(spl_object_hash($a)), "\n";
echo (spl_object_hash($a) === spl_object_hash($a)) ? "stable\n" : "unstable\n";
echo (spl_object_hash($a) !== spl_object_hash($b)) ? "distinct\n" : "same\n";
?>
--EXPECT--
32
stable
distinct
--CLEAN--
<?php
