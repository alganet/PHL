--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: PDO::SQLITE_* still exist and answer, emitting E_DEPRECATED that names the successor (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* the box's php.ini masks E_DEPRECATED, so the emission is captured rather
 * than printed */
set_error_handler(function ($no, $str) { echo "[$no] $str\n"; return true; });
$r = new ReflectionClass('PDO');
foreach (['SQLITE_DETERMINISTIC', 'SQLITE_ATTR_OPEN_FLAGS', 'SQLITE_OPEN_READONLY',
          'SQLITE_OPEN_READWRITE', 'SQLITE_OPEN_CREATE', 'SQLITE_ATTR_READONLY_STATEMENT',
          'SQLITE_ATTR_EXTENDED_RESULT_CODES'] as $name) {
    var_dump($r->getReflectionConstant($name)->isDeprecated());
}
var_dump(defined('PDO::SQLITE_OPEN_READONLY'));
var_dump(PDO::SQLITE_DETERMINISTIC);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
[8192] Constant PDO::SQLITE_DETERMINISTIC is deprecated since 8.5, use Pdo\Sqlite::DETERMINISTIC instead
int(2048)
