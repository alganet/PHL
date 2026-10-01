--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: an ini value php's parser refuses still stores the piece read before the refusal (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--DESCRIPTION--
php's ini grammar reduces a value expression as soon as the next byte cannot
extend it, and only then discovers that byte starts nothing of its own --
`On~2` stores boolean "1" before php's parser calls the leftover `~` a syntax
error. PHL now stores the same "1"; it does not yet raise php's own
`syntax error, unexpected ...` warning for the refusal (see the `_zend` half).
--INI--
display_errors=On~2
--FILE--
<?php
var_dump(ini_get('display_errors'));
?>
--EXPECT--
string(1) "1"
