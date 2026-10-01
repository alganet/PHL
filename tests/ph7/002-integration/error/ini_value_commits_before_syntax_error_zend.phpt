--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a refused ini value still stores the piece read before the refusal, and warns (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--INI--
display_errors=On~2
--FILE--
<?php
var_dump(ini_get('display_errors'));
?>
--EXPECTF--
PHP:  syntax error, unexpected '~' in %s on line %d
string(1) "1"
