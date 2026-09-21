--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file_get_contents with an explicit zero length reads nothing; NULL reads the whole file
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_fgc0');
file_put_contents($fn, 'Hello World');
var_dump(file_get_contents($fn, false, null, 0, 0));
var_dump(file_get_contents($fn, false, null, 3, 0));
var_dump(file_get_contents($fn, false, null, 0, null));
var_dump(file_get_contents($fn, false, null, 6, 5));
?>
--EXPECT--
string(0) ""
string(0) ""
string(11) "Hello World"
string(5) "World"
--CLEAN--
<?php
if (isset($fn) && file_exists($fn)) unlink($fn);
unset($fn);
