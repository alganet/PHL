--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ENVIRONMENT DIVERGENCE The embedded database names the release it was cut from (PHL half)
--DESCRIPTION--
timezone_version_get() answers timelib's spelling of the IANA release behind the database: the
four-digit year, a dot, and the release LETTER as a 1-based number, so `2026c` prints `2026.3`.
PHL embeds its own table and answers for that. The oracle on the build box is
--with-system-tzdata -- it reads the platform's zoneinfo, which carries no release string, and
answers the constant `0.system` for any tree. That is a property of how the oracle was
configured, not a divergence in the answer's shape; a php built with its own bundled database
prints the same `YYYY.N` this does. The php half is in the _zend member.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend member";
}
?>
--FILE--
<?php
$v = timezone_version_get();
var_dump($v);
var_dump((bool) preg_match('/^\d{4}\.\d+$/', $v));
?>
--EXPECT--
string(6) "2026.3"
bool(true)
