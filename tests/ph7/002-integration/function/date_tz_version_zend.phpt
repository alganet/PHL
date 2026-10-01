--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ENVIRONMENT DIVERGENCE A --with-system-tzdata build has no release to name (php half)
--DESCRIPTION--
The php half of date_tz_version.phpt. This oracle reads the platform's zoneinfo rather than a
bundled table, so there is no release string to answer and timelib substitutes the constant
`0.system`. PHL's own answer is in the non-_zend member.
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
} elseif (timezone_version_get() !== '0.system') {
    echo "skip this php has a bundled timezone database";
}
?>
--FILE--
<?php
var_dump(timezone_version_get());
?>
--EXPECT--
string(8) "0.system"
