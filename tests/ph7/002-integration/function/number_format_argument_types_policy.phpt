--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: number_format() rejects a null / lossy-float argument (PHL half)
--DESCRIPTION--
php only DEPRECATES null passed to a non-nullable scalar parameter, and a lossy float passed
to an int one, then coerces. PHL targets php's non-deprecated surface (§10) and throws the ZPP
TypeError instead -- the same answer str_repeat() already gives for the same two shapes. The
two SEPARATORS are declared `?string`, so null is legal there and stays legal. php's half is
the `_zend` twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
function t($label, $fn) {
    echo "$label: ";
    try { var_dump($fn()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
t('null $num',       fn() => number_format(null));
t('null $decimals',  fn() => number_format(1234.5, null));
t('lossy float',     fn() => number_format(1234.5, 2.7));
t('null separators', fn() => number_format(1234.5, 2, null, null));
?>
--EXPECT--
null $num: TypeError: number_format(): Argument #1 ($num) must be of type int|float, null given
null $decimals: TypeError: number_format(): Argument #2 ($decimals) must be of type int, null given
lossy float: TypeError: number_format(): Argument #2 ($decimals) must be of type int, float given
null separators: string(8) "1,234.50"
