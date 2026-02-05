--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
utf8_decode decodes UTF-8 to ISO-8859-1
--SKIPIF--
<?php
if (defined('PHP_VERSION_ID') && PHP_VERSION_ID >= 80300) {
	echo "skip";
}
?>
--FILE--
<?php
$old = error_reporting();
error_reporting($old & ~E_DEPRECATED);
$utf8 = "\xC3\xA9"; // 'é'
$latin = @utf8_decode($utf8);
echo ($latin === "\xE9") ? "ok\n" : "fail\n";
error_reporting($old);
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($old, $utf8, $latin);
