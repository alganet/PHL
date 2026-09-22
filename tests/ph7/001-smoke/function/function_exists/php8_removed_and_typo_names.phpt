--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_real() and the douleval typo are not defined (php-8 surface, §10)
--DESCRIPTION--
The registration table used to carry two names php does not define: is_real(),
which php REMOVED in 8.0 (§10 keeps no deprecated surface), and "douleval", a
misspelling of doubleval() — the real doubleval() lives in the prelude, so the
typo was pure PHL-only surface. Their php-defined neighbours must stay.
--FILE--
<?php
foreach (['is_real', 'douleval'] as $gone) {
    var_dump(function_exists($gone));
}
foreach (['is_float', 'is_double', 'floatval', 'doubleval'] as $kept) {
    var_dump(function_exists($kept));
}
var_dump(is_double(1.5), doubleval('2.5'));
?>
--EXPECT--
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
float(2.5)
--CLEAN--
<?php
