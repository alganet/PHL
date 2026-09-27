--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: createFromTimestamp(null) deprecates and answers the epoch (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php's own answer for the argument §10 refuses: an E_DEPRECATED (muted here,
 * as everywhere in this corpus) and the epoch. */
error_reporting(E_ALL & ~E_DEPRECATED);
date_default_timezone_set('UTC');
foreach (['DateTime', 'DateTimeImmutable'] as $cls) {
    try {
        var_dump($cls::createFromTimestamp(null)->format('c'));
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
string(25) "1970-01-01T00:00:00+00:00"
string(25) "1970-01-01T00:00:00+00:00"
