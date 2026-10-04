--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `self`/`parent` callable spelling forwards the caller's called class
--DESCRIPTION--
In a callback, `'parent::m'`, `['parent','m']`, `'self::m'` and `['self','m']`
forward like the `parent::m()` syntax: the method the keyword resolves to runs,
and `static::` inside it is the caller's late-static-binding class whenever that
class is one of the resolved class's descendants. The same holds for a name the
class answers only through __callStatic. An explicit class name does not forward.
--FILE--
<?php
class FcsA {
    public static function sm() { return "FcsA::sm " . static::class; }
    public static function __callStatic($n, $a) { return "FcsA::__callStatic($n) " . static::class; }
}
class FcsB extends FcsA {
    public static function sm() { return "FcsB::sm " . static::class; }
    function viaInstance() { return self::run(); }
    static function viaStatic() { return self::run(); }
    static function run() {
        $out = [];
        foreach (['parent::sm', ['parent', 'sm'], 'self::sm', ['self', 'sm'], 'static::sm',
                  'FcsA::sm', ['FcsA', 'sm'], 'parent::missing', ['parent', 'missing'],
                  'self::missing', 'FcsA::missing'] as $cb) {
            $out[] = (is_array($cb) ? implode(',', $cb) : $cb) . ' => ' . call_user_func($cb);
        }
        $out[] = 'array_map parent::sm => ' . array_map('parent::sm', [1])[0];
        $out[] = 'call_user_func_array self,sm => ' . call_user_func_array(['self', 'sm'], []);
        return implode("\n", $out) . "\n";
    }
}
class FcsC extends FcsB {}
echo "-- a FcsC instance\n", (new FcsC)->viaInstance();
echo "-- FcsC::viaStatic()\n", FcsC::viaStatic();
echo "-- FcsB::viaStatic()\n", FcsB::viaStatic();
--EXPECT--
-- a FcsC instance
parent::sm => FcsA::sm FcsC
parent,sm => FcsA::sm FcsC
self::sm => FcsB::sm FcsC
self,sm => FcsB::sm FcsC
static::sm => FcsB::sm FcsC
FcsA::sm => FcsA::sm FcsA
FcsA,sm => FcsA::sm FcsA
parent::missing => FcsA::__callStatic(missing) FcsC
parent,missing => FcsA::__callStatic(missing) FcsC
self::missing => FcsA::__callStatic(missing) FcsC
FcsA::missing => FcsA::__callStatic(missing) FcsA
array_map parent::sm => FcsA::sm FcsC
call_user_func_array self,sm => FcsB::sm FcsC
-- FcsC::viaStatic()
parent::sm => FcsA::sm FcsC
parent,sm => FcsA::sm FcsC
self::sm => FcsB::sm FcsC
self,sm => FcsB::sm FcsC
static::sm => FcsB::sm FcsC
FcsA::sm => FcsA::sm FcsA
FcsA,sm => FcsA::sm FcsA
parent::missing => FcsA::__callStatic(missing) FcsC
parent,missing => FcsA::__callStatic(missing) FcsC
self::missing => FcsA::__callStatic(missing) FcsC
FcsA::missing => FcsA::__callStatic(missing) FcsA
array_map parent::sm => FcsA::sm FcsC
call_user_func_array self,sm => FcsB::sm FcsC
-- FcsB::viaStatic()
parent::sm => FcsA::sm FcsB
parent,sm => FcsA::sm FcsB
self::sm => FcsB::sm FcsB
self,sm => FcsB::sm FcsB
static::sm => FcsB::sm FcsB
FcsA::sm => FcsA::sm FcsA
FcsA,sm => FcsA::sm FcsA
parent::missing => FcsA::__callStatic(missing) FcsB
parent,missing => FcsA::__callStatic(missing) FcsB
self::missing => FcsA::__callStatic(missing) FcsB
FcsA::missing => FcsA::__callStatic(missing) FcsA
array_map parent::sm => FcsA::sm FcsB
call_user_func_array self,sm => FcsB::sm FcsB
