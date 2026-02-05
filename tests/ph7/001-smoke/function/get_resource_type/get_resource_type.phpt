--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_resource_type returns a well-formed resource id string
--SKIPIF--
<?php
if (function_exists('zend_version')) {
	echo "skip";
}
?>
--FILE--
<?php
$f = fopen(__FILE__, 'r');
$t = get_resource_type($f);
if (strpos($t, 'resID_') === 0) echo "ok\n"; else echo "fail\n";
fclose($f);
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($f, $t);
