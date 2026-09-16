--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
utf8_decode decodes UTF-8 to ISO-8859-1
--SKIPIF--
<?php
// php DEPRECATES utf8_decode() since 8.2 (E_DEPRECATED, caught by the runner's error
// handler); PHL still exposes it as non-deprecated surface. It is scheduled to be
// SUPERSEDED by the mb_* equivalent (mb_convert_encoding) and then removed; this guard
// retires with that ship.
if (function_exists('zend_version')) { echo 'skip php deprecates utf8_decode() since 8.2; PHL supersedes it with mb_* later'; }
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
