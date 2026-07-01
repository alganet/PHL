--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP 8.5: round() accepts the integer modes 5..8 (CEILING/FLOOR/TOWARD_ZERO/AWAY_FROM_ZERO)
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.5.0', '<')) echo 'skip Requires PHP 8.5+'; ?>
--FILE--
<?php
// PHP 8.5 accepts the integer rounding modes 5..8 (CEILING/FLOOR/
// TOWARD_ZERO/AWAY_FROM_ZERO) even without the RoundingMode enum. These
// integer values are not a public API before 8.4 (older PHP silently
// treats them as HALF_UP), so this test is gated to PHP 8.5+; PHL runs it
// unconditionally (function_exists('zend_version') is false in PHL).
foreach ([5, 6, 7, 8] as $m) {
    printf("mode%d round(2.5)=%s round(-2.5)=%s\n", $m, var_export(round(2.5, 0, $m), true), var_export(round(-2.5, 0, $m), true));
}
?>
--EXPECT--
mode5 round(2.5)=3.0 round(-2.5)=-2.0
mode6 round(2.5)=2.0 round(-2.5)=-3.0
mode7 round(2.5)=2.0 round(-2.5)=-2.0
mode8 round(2.5)=3.0 round(-2.5)=-3.0
