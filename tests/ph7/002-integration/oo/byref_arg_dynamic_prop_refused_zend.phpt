--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a by-reference argument creates the dynamic property behind a deprecation (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--INI--
error_reporting=E_ALL & ~E_DEPRECATED
--FILE--
<?php
/* php's answer to the same six: an opted-in class and stdClass really do allow
 * the property, and every other class gets it too -- behind `Creation of dynamic
 * property C::$m` (E_DEPRECATED, masked here), which §10 turns into the Error the
 * PHL half pins. The `__set`-with-no-`__get` row is php's own fetch rule: only
 * `__get` can answer a W fetch, so `__set` alone falls through to this path. */
#[\AllowDynamicProperties] class BdpOpen {}
class BdpOpenKid extends BdpOpen {}
class BdpClosed {}
class BdpSetOnly { public function __set($n, $v) {} }

function bdp(string $label, callable $fn): void {
    try { $out = $fn(); } catch (\Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-28s -> %s\n", $label, str_replace("\n", '', var_export($out, true)));
}
bdp('stdClass', function () { $o = new stdClass; preg_match('/(a)/', 'a', $o->m); return $o->m ?? '<none>'; });
bdp('AllowDynamicProperties', function () { $o = new BdpOpen; preg_match('/(a)/', 'a', $o->m); return $o->m ?? '<none>'; });
bdp('inherited from the parent', function () { $o = new BdpOpenKid; preg_match('/(a)/', 'a', $o->m); return $o->m ?? '<none>'; });
bdp('a plain class', function () { $o = new BdpClosed; preg_match('/(a)/', 'a', $o->m); return $o->m ?? '<none>'; });
bdp('__set with no __get', function () { $o = new BdpSetOnly; preg_match('/(a)/', 'a', $o->m); return $o->m ?? '<none>'; });
bdp('a user function too', function () { $o = new BdpClosed; $f = function (&$x) { $x = 1; }; $f($o->m); return $o->m ?? '<none>'; });
--EXPECT--
stdClass                     -> array (  0 => 'a',  1 => 'a',)
AllowDynamicProperties       -> array (  0 => 'a',  1 => 'a',)
inherited from the parent    -> array (  0 => 'a',  1 => 'a',)
a plain class                -> array (  0 => 'a',  1 => 'a',)
__set with no __get          -> array (  0 => 'a',  1 => 'a',)
a user function too          -> 1
