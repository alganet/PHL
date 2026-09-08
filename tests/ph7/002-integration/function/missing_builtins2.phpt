--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Second missing-builtin wave, and M_PI to full double precision
--FILE--
<?php
function p($l, $fn) { try { echo "$l => ", var_export($fn(), true), "\n"; } catch (Throwable $e) { echo "$l => ", get_class($e), ": ", $e->getMessage(), "\n"; } }
p('is_iterable',   fn() => [is_iterable([1]), is_iterable(new ArrayObject([])), is_iterable(5)]);
p('is_countable',  fn() => [is_countable([1]), is_countable(new ArrayObject([])), is_countable('x')]);
p('key_exists',    fn() => [key_exists('a', ['a'=>1]), key_exists('b', ['a'=>1])]);
p('doubleval',     fn() => doubleval('3.5abc'));
p('array_count_values', fn() => array_count_values(['a','b','a',1,1]));
p('array_change_key_case', fn() => array_change_key_case(['aB'=>1,'Cd'=>2], CASE_UPPER));
p('array_replace_recursive', fn() => array_replace_recursive(['a'=>['x'=>1,'y'=>2]], ['a'=>['y'=>9,'z'=>3]]));
p('class_uses',    fn() => class_uses('ArrayObject'));
p('count_chars 1', fn() => count_chars('aab', 1));
p('count_chars 3', fn() => count_chars('aab', 3));
p('ip2long',       fn() => ip2long('192.168.1.1'));
p('long2ip',       fn() => long2ip(3232235777));
p('ip2long bad',   fn() => ip2long('999.1.1.1'));
p('preg_filter',   fn() => preg_filter('/^a/', 'X', ['apple','banana','avocado']));
p('preg_rcb_array',fn() => preg_replace_callback_array(['/a/' => fn($m) => 'A', '/b/' => fn($m) => 'B'], 'abc'));
p('deg2rad',       fn() => deg2rad(180));
p('rad2deg',       fn() => rad2deg(M_PI));
p('log1p',         fn() => log1p(1e-10));
p('expm1',         fn() => expm1(1e-10));
// libm differs by 1 ULP across platforms (macOS vs glibc) and php inherits it,
// so compare at a precision both agree on.
p('asinh/acosh',   fn() => array_map(fn($x) => sprintf('%.12f', $x), [asinh(1.0), acosh(2.0), atanh(0.5)]));
p('fpow',          fn() => fpow(2, 10));
// PH7_PI was 3.1415926535898 -- 14 significant digits -- so M_PI differed from php's
// in the 13th place and everything built on it was quietly off.
p('M_PI exact',    fn() => sprintf('%.17g', M_PI));
p('pi() exact',    fn() => sprintf('%.17g', pi()));
p('rad2deg(M_PI)', fn() => rad2deg(M_PI));
?>
--EXPECT--
is_iterable => array (
  0 => true,
  1 => true,
  2 => false,
)
is_countable => array (
  0 => true,
  1 => true,
  2 => false,
)
key_exists => array (
  0 => true,
  1 => false,
)
doubleval => 3.5
array_count_values => array (
  'a' => 2,
  'b' => 1,
  1 => 2,
)
array_change_key_case => array (
  'AB' => 1,
  'CD' => 2,
)
array_replace_recursive => array (
  'a' => 
  array (
    'x' => 1,
    'y' => 9,
    'z' => 3,
  ),
)
class_uses => array (
)
count_chars 1 => array (
  97 => 2,
  98 => 1,
)
count_chars 3 => 'ab'
ip2long => 3232235777
long2ip => '192.168.1.1'
ip2long bad => false
preg_filter => array (
  0 => 'Xpple',
  2 => 'Xvocado',
)
preg_rcb_array => 'ABc'
deg2rad => 3.141592653589793
rad2deg => 180.0
log1p => 9.999999999500001E-11
expm1 => 1.00000000005E-10
asinh/acosh => array (
  0 => '0.881373587020',
  1 => '1.316957896925',
  2 => '0.549306144334',
)
fpow => 1024.0
M_PI exact => '3.1415926535897931'
pi() exact => '3.1415926535897931'
rad2deg(M_PI) => 180.0
--CLEAN--
<?php
