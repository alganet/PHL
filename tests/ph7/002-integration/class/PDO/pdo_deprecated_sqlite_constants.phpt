--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: the seven deprecated PDO::SQLITE_* constants are absent; only the Pdo\Sqlite successors exist (PHL half of the twin pair)
--DESCRIPTION--
php 8.5 still declares PDO::SQLITE_DETERMINISTIC and six siblings, every one of
them reporting ReflectionClassConstant::isDeprecated() and pointing at an
unprefixed Pdo\Sqlite spelling. §10's non-deprecated policy is to REMOVE what
php merely deprecates, so PHL declares the successors only and a read of the old
name is the engine's ordinary undefined-constant Error.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
$r = new ReflectionClass('PDO');
foreach (['SQLITE_DETERMINISTIC', 'SQLITE_ATTR_OPEN_FLAGS', 'SQLITE_OPEN_READONLY',
          'SQLITE_OPEN_READWRITE', 'SQLITE_OPEN_CREATE', 'SQLITE_ATTR_READONLY_STATEMENT',
          'SQLITE_ATTR_EXTENDED_RESULT_CODES'] as $name) {
    var_dump($r->hasConstant($name));
}
var_dump(defined('PDO::SQLITE_OPEN_READONLY'));
try { var_dump(PDO::SQLITE_DETERMINISTIC); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* the successors carry php's own values */
var_dump(Pdo\Sqlite::DETERMINISTIC, Pdo\Sqlite::OPEN_READONLY, Pdo\Sqlite::OPEN_READWRITE,
    Pdo\Sqlite::OPEN_CREATE, Pdo\Sqlite::ATTR_OPEN_FLAGS, Pdo\Sqlite::ATTR_READONLY_STATEMENT,
    Pdo\Sqlite::ATTR_EXTENDED_RESULT_CODES);
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
Error: Undefined constant PDO::SQLITE_DETERMINISTIC
int(2048)
int(1)
int(2)
int(4)
int(1000)
int(1001)
int(1002)
