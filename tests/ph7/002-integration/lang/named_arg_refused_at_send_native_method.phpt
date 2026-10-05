--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named argument to a built-in class's method is refused where it is sent
--FILE--
<?php
/* php resolves a NAME at the send of its own argument, and a method a built-in
 * class declares in C binds by its declared parameter names like any other: an
 * unknown name, or one a positional argument already filled, throws before a
 * LATER argument runs and before a plain variable operand is read. A method
 * that declares no parameter refuses every name rather than counting it. */
set_error_handler(function ($no, $msg) { echo "  warning: $msg\n"; return true; });
function s($t) { echo "  ran $t\n"; return 1; }
function t($label, $c) {
    echo "$label\n";
    try { $r = $c(); echo "  = ", var_export($r, true), "\n"; }
    catch (\Error $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
class Sub extends ArrayObject {
    function up() { return parent::count(zz: $u, a: s('parent')); }
}
$ao = new ArrayObject([3, 1, 2]);
$d = new DateTime('2020-01-01 00:00:00', new DateTimeZone('UTC'));
t('no parameter', fn() => $ao->count(zz: $u, a: s('count')));
t('unknown', fn() => $ao->offsetGet(zz: $u, key: s('offsetGet')));
t('known', fn() => $ao->offsetGet(key: 1));
t('overwrites', fn() => $ao->offsetSet(1, key: $u, value: s('offsetSet')));
t('overwrites after an unread variable', fn() => $ao->offsetSet($u2, key: 1));
t('overwrites a positional', fn() => $d->setTime(1, hour: s('setTime')));
t('reordered', fn() => $d->setTime(minute: 5, hour: 2)->format('H:i'));
t('static unknown', fn() => DateTime::createFromFormat(zz: $u, format: s('static')));
t('static reordered', fn() => DateTime::createFromFormat(datetime: '2020', format: 'Y')->format('Y'));
t('nullsafe', fn() => $ao?->count(zz: $u));
t('parent::', fn() => (new Sub)->up());
t('reflection', fn() => (new ReflectionClass('ArrayObject'))->getMethod(zz: $u, name: s('getMethod')));
--EXPECT--
no parameter
  Error: Unknown named parameter $zz
unknown
  Error: Unknown named parameter $zz
known
  = 1
overwrites
  Error: Named parameter $key overwrites previous argument
overwrites after an unread variable
  warning: Undefined variable $u2
  Error: Named parameter $key overwrites previous argument
overwrites a positional
  ran setTime
  Error: Named parameter $hour overwrites previous argument
reordered
  = '02:05'
static unknown
  Error: Unknown named parameter $zz
static reordered
  = '2020'
nullsafe
  Error: Unknown named parameter $zz
parent::
  Error: Unknown named parameter $zz
reflection
  Error: Unknown named parameter $zz
