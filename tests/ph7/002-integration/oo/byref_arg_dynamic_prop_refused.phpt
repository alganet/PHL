--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a by-reference argument refuses the dynamic property its write path refuses (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* A by-reference out-parameter reaching a property a class neither declares nor
 * opts into is php's E_DEPRECATED `Creation of dynamic property C::$m` and a
 * created property; the scope policy rejects that whole surface, so this raises the same
 * Error `$o->m = 1` raises. stdClass and #[\AllowDynamicProperties] are
 * unaffected -- they really do allow one -- and so is a class with `__set` but
 * no `__get`, which is where php's fetch gives up and creates one. */
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
a plain class                -> 'Error: Cannot create dynamic property BdpClosed::$m'
__set with no __get          -> 'Error: Cannot create dynamic property BdpSetOnly::$m'
a user function too          -> 'Error: Cannot create dynamic property BdpClosed::$m'
