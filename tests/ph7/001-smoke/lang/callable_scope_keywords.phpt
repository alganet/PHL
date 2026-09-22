--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self/parent/static resolve in a CALLBACK, and stay unresolved in a direct call
--DESCRIPTION--
php's answer for a callable naming a scope keyword is SPLIT, and PHL answered
"no" to both halves. In a CALLBACK — is_callable, call_user_func, array_map,
usort — `'self::m'` and `['parent','m']` resolve against the live class context,
with `static::` following late static binding. In the DIRECT `$cb()` dispatch
php resolves nothing and reports `Class "self" not found`, because the dynamic
call opcode never looks at the class context; the same goes for a first-class
callable made from such a value. Both halves are pinned here, since making one
work is exactly what would break the other.
--FILE--
<?php
class CskBase {
    public static function s() { return 'CskBase::s'; }
    public function m() { return 'CskBase::m ' . (isset($this) ? get_class($this) : 'none'); }
}
class CskChild extends CskBase {
    public static function s() { return 'CskChild::s'; }

    public function callbacks(): array
    {
        $out = [];
        foreach (['self::s', 'parent::s', 'static::s', 'self::m'] as $name) {
            $pair = explode('::', $name);
            $out["is_callable str $name"] = is_callable($name);
            $out["is_callable arr $name"] = is_callable($pair);
            $out["call_user_func str $name"] = call_user_func($name);
            $out["call_user_func arr $name"] = call_user_func($pair);
            $out["array_map $name"] = array_map($name, [1])[0];
        }
        $out['is_callable missing'] = is_callable('self::nope');
        $out['name out-param'] = null;
        is_callable('static::s', false, $out['name out-param']);
        return $out;
    }

    public function directDispatch(): array
    {
        $out = [];
        foreach (['self::s', 'parent::s', 'static::s'] as $name) {
            try {
                $cb = $name;
                $out["direct $name"] = $cb();
            } catch (Throwable $e) {
                $out["direct $name"] = get_class($e) . ': ' . $e->getMessage();
            }
            try {
                $cb = explode('::', $name);
                $out["direct arr $name"] = $cb();
            } catch (Throwable $e) {
                $out["direct arr $name"] = get_class($e) . ': ' . $e->getMessage();
            }
            try {
                $cb = $name;
                $fcc = $cb(...);
                $out["fcc $name"] = $fcc();
            } catch (Throwable $e) {
                $out["fcc $name"] = get_class($e) . ': ' . $e->getMessage();
            }
        }
        return $out;
    }
}

foreach ((new CskChild)->callbacks() as $k => $v) { echo $k, ' => ', var_export($v, true), "\n"; }
foreach ((new CskChild)->directDispatch() as $k => $v) { echo $k, ' => ', var_export($v, true), "\n"; }
/* No class scope: nothing to resolve against. */
echo 'global is_callable => ', var_export(is_callable('self::s'), true), "\n";
echo 'global is_callable arr => ', var_export(is_callable(['self', 's']), true), "\n";
echo "end\n";
?>
--EXPECT--
is_callable str self::s => true
is_callable arr self::s => true
call_user_func str self::s => 'CskChild::s'
call_user_func arr self::s => 'CskChild::s'
array_map self::s => 'CskChild::s'
is_callable str parent::s => true
is_callable arr parent::s => true
call_user_func str parent::s => 'CskBase::s'
call_user_func arr parent::s => 'CskBase::s'
array_map parent::s => 'CskBase::s'
is_callable str static::s => true
is_callable arr static::s => true
call_user_func str static::s => 'CskChild::s'
call_user_func arr static::s => 'CskChild::s'
array_map static::s => 'CskChild::s'
is_callable str self::m => true
is_callable arr self::m => true
call_user_func str self::m => 'CskBase::m CskChild'
call_user_func arr self::m => 'CskBase::m CskChild'
array_map self::m => 'CskBase::m CskChild'
is_callable missing => false
name out-param => 'static::s'
direct self::s => 'Error: Class "self" not found'
direct arr self::s => 'Error: Class "self" not found'
fcc self::s => 'Error: Class "self" not found'
direct parent::s => 'Error: Class "parent" not found'
direct arr parent::s => 'Error: Class "parent" not found'
fcc parent::s => 'Error: Class "parent" not found'
direct static::s => 'Error: Class "static" not found'
direct arr static::s => 'Error: Class "static" not found'
fcc static::s => 'Error: Class "static" not found'
global is_callable => false
global is_callable arr => false
end
