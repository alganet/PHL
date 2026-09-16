--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
utf8_encode encodes ISO-8859-1 to UTF-8
--SKIPIF--
<?php
// php DEPRECATES utf8_encode() since 8.2 (E_DEPRECATED, caught by the runner's error
// handler); PHL still exposes it as non-deprecated surface. It is scheduled to be
// SUPERSEDED by the mb_* equivalent (mb_convert_encoding) and then removed; this guard
// retires with that ship.
if (function_exists('zend_version')) { echo 'skip php deprecates utf8_encode() since 8.2; PHL supersedes it with mb_* later'; }
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
