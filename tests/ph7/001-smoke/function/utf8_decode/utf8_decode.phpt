--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
utf8_decode decodes UTF-8 to ISO-8859-1
--SKIPIF--
<?php
// php DEPRECATES utf8_decode() since 8.2 (E_DEPRECATED, caught by the runner's error
// handler); PHL still exposes it as non-deprecated surface. The scope policy says PHL
// REMOVES what php merely deprecates -- removal is filed there as an open decision,
// and this guard retires with it.
if (function_exists('zend_version')) { echo 'skip php deprecates utf8_decode() since 8.2; PHL has not yet removed it'; }
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
