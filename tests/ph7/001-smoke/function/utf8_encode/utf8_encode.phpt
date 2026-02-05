--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
utf8_encode encodes ISO-8859-1 to UTF-8
--SKIPIF--
<?php
if (defined('PHP_VERSION_ID') && PHP_VERSION_ID >= 80300) {
	echo "skip";
}
?>
--FILE--
<?php
// Latin-1 char: 0xE9 (é)
$latin = "\xE9";
$utf8 = utf8_encode($latin);
// Expected UTF-8 sequence for 'é' is 0xC3 0xA9
$expected = "\xC3\xA9";
echo ($utf8 === $expected) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($latin, $utf8, $expected);
