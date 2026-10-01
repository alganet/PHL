--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An argument diagnostic names a call SITE only when the frame above is user code
--FILE--
<?php
/* php reads prev_execute_data and asks whether it is USER code — it does not walk
 * past an internal frame. So a callback array_map() reached for has that builtin's
 * frame above it and gets no `, called in FILE on line N` tail, while a direct call
 * does. call_user_func() is not an exception: php's compiler elides its frame, so
 * the frame above the callee really is the caller's. */
function cbSiteFn(int $i) {}
function cbSiteFew(int $a, int $b) {}
class CbSiteK { public function m(int $i) {} public static function s(int $i) {} }

$say = static function (string $label, callable $f): void {
    try { $f(); } catch (\Throwable $e) {
        echo $label, ' | ', get_class($e), ': ',
             str_replace(__FILE__, 'F', $e->getMessage()), "\n";
    }
};
$k = new CbSiteK;

$say('direct',        static fn() => cbSiteFn('a'));
$say('direct-meth',   static fn() => $k->m('a'));
$say('direct-few',    static fn() => cbSiteFew(1));
$say('cuf',           static fn() => call_user_func('cbSiteFn', 'a'));
$say('cufa',          static fn() => call_user_func_array('cbSiteFn', ['a']));
$say('fcc',           static function () { $f = cbSiteFn(...); $f('a'); });

$say('array_map',     static fn() => array_map('cbSiteFn', ['a']));
$say('array_map-meth',static fn() => array_map([$k, 'm'], ['a']));
$say('array_filter',  static fn() => array_filter(['a'], 'cbSiteFn'));
$say('array_walk',    static function () { $a = ['a']; array_walk($a, 'cbSiteFn'); });
$say('usort',         static function () { $a = ['b', 'a']; usort($a, 'cbSiteFn'); });
$say('preg_cb',       static fn() => preg_replace_callback('/x/', 'cbSiteFn', 'x'));
$say('map-few',       static fn() => array_map('cbSiteFew', [1]));
$say('refl-invoke',   static fn() => (new ReflectionFunction('cbSiteFn'))->invoke('a'));
?>
--EXPECT--
direct | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, string given, called in F on line 19
direct-meth | TypeError: CbSiteK::m(): Argument #1 ($i) must be of type int, string given, called in F on line 20
direct-few | ArgumentCountError: Too few arguments to function cbSiteFew(), 1 passed in F on line 21 and exactly 2 expected
cuf | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, string given, called in F on line 22
cufa | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, string given, called in F on line 23
fcc | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, string given, called in F on line 24
array_map | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, string given
array_map-meth | TypeError: CbSiteK::m(): Argument #1 ($i) must be of type int, string given
array_filter | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, string given
array_walk | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, string given
usort | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, string given
preg_cb | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, array given
map-few | ArgumentCountError: Too few arguments to function cbSiteFew(), 1 passed and exactly 2 expected
refl-invoke | TypeError: cbSiteFn(): Argument #1 ($i) must be of type int, string given
--CLEAN--
<?php
unset($say, $k);
