--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: number_format() deprecates and coerces a null / lossy-float argument (php half)
--DESCRIPTION--
The php half of number_format_argument_types_policy.phpt: php coerces null to 0 and truncates
a lossy float, each with an E_DEPRECATED. PHL throws the ZPP TypeError instead (§10).
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });
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
null $num:   [8192] number_format(): Passing null to parameter #1 ($num) of type float is deprecated
string(1) "0"
null $decimals:   [8192] number_format(): Passing null to parameter #2 ($decimals) of type int is deprecated
string(5) "1,235"
lossy float:   [8192] Implicit conversion from float 2.7 to int loses precision
string(8) "1,234.50"
null separators: string(8) "1,234.50"
