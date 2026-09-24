--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: stream_get_transports() names the whole registered set (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
$t = stream_get_transports();
var_dump(in_array('tcp', $t, true));
/* php registers the transports its build has; the ones PHL does not model are
 * exactly what the PHL half records as missing. */
var_dump(count($t) > 1);
?>
--EXPECT--
bool(true)
bool(true)
