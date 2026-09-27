--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: createFromTimestamp(null) is a TypeError (§10 twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair";
}
?>
--FILE--
<?php
/* php only DEPRECATES null for a non-nullable internal scalar parameter and
 * carries on with 0, so `createFromTimestamp(null)` is the epoch there. §10
 * rejects what php merely deprecates, so it is this engine's TypeError -- the
 * same refusal every other non-nullable scalar parameter gives. The zend half
 * of this pair records php's answer. */
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
TypeError: DateTime::createFromTimestamp(): Argument #1 ($timestamp) must be of type int|float, null given
TypeError: DateTimeImmutable::createFromTimestamp(): Argument #1 ($timestamp) must be of type int|float, null given
