--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_object_id() is stable per object and distinct across objects
--FILE--
<?php
class C {}
$a = new C; $b = new C;
echo (spl_object_id($a) === spl_object_id($a)) ? "stable\n" : "unstable\n";
echo (spl_object_id($a) !== spl_object_id($b)) ? "distinct\n" : "same\n";
echo function_exists('spl_object_id') ? "exists\n" : "missing\n";
?>
--EXPECT--
stable
distinct
exists
--CLEAN--
<?php
