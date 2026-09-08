--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Builtins PH7 never had: number_format, hex2bin, fdiv, checkdate, preg_grep, class_implements
--FILE--
<?php
// Functions PH7 never had. A function_exists() sweep against php found eleven
// missing outright; number_format() alone is one of the most-used in php.
function nbTry($label, $fn)
{
    try {
        echo $label, ' => ', var_export($fn(), true), "\n";
    } catch (Throwable $e) {
        echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}
interface NbI {}
class NbBase implements NbI {}
class NbChild extends NbBase {}

nbTry('number_format',       fn() => number_format(1234.5678, 2));
nbTry('number_format bare',  fn() => number_format(1234.5678));
nbTry('number_format seps',  fn() => number_format(1234567.891, 2, ',', '.'));
nbTry('number_format neg',   fn() => number_format(-1234.5, 1));
nbTry('number_format half',  fn() => number_format(0.5));
nbTry('number_format nosep', fn() => number_format(1234.5, 2, '.', ''));
nbTry('hex2bin',             fn() => hex2bin('48656c6c6f'));
nbTry('hex2bin roundtrip',   fn() => hex2bin(bin2hex("PHL\x00\xff")) === "PHL\x00\xff");
nbTry('hex2bin odd',         fn() => @hex2bin('abc'));
nbTry('fdiv +',              fn() => fdiv(1, 0));
nbTry('fdiv -',              fn() => fdiv(-1, 0));
nbTry('fdiv nan',            fn() => is_nan(fdiv(0, 0)));
nbTry('fdiv normal',         fn() => fdiv(7, 2));
nbTry('is_finite family',    fn() => [is_finite(1.0), is_finite(INF), is_infinite(-INF), is_nan(NAN)]);
nbTry('checkdate',           fn() => [checkdate(2, 29, 2024), checkdate(2, 30, 2024), checkdate(2, 29, 2023), checkdate(13, 1, 2020)]);
nbTry('cal_days_in_month',   fn() => [cal_days_in_month(CAL_GREGORIAN, 2, 2024), cal_days_in_month(CAL_GREGORIAN, 2, 2023)]);
nbTry('preg_grep',           fn() => preg_grep('/^a/', ['apple', 'banana', 'avocado']));
nbTry('preg_grep invert',    fn() => preg_grep('/^a/', ['apple', 'banana'], PREG_GREP_INVERT));
nbTry('class_implements',    fn() => class_implements(new NbChild()));
nbTry('class_parents',       fn() => class_parents('NbChild'));
?>
--EXPECT--
number_format => '1,234.57'
number_format bare => '1,235'
number_format seps => '1.234.567,89'
number_format neg => '-1,234.5'
number_format half => '1'
number_format nosep => '1234.50'
hex2bin => 'Hello'
hex2bin roundtrip => true
hex2bin odd => false
fdiv + => INF
fdiv - => -INF
fdiv nan => true
fdiv normal => 3.5
is_finite family => array (
  0 => true,
  1 => false,
  2 => true,
  3 => true,
)
checkdate => array (
  0 => true,
  1 => false,
  2 => false,
  3 => false,
)
cal_days_in_month => array (
  0 => 29,
  1 => 28,
)
preg_grep => array (
  0 => 'apple',
  2 => 'avocado',
)
preg_grep invert => array (
  1 => 'banana',
)
class_implements => array (
  'NbI' => 'NbI',
)
class_parents => array (
  'NbBase' => 'NbBase',
)
--CLEAN--
<?php
