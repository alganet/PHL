--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An error handler that throws on the callable-scope deprecation stops the door that raised it
--DESCRIPTION--
php raises `Use of "self" in callables is deprecated` while it resolves the callable, and a
handler that throws there makes the callable unresolved: the callback never runs, nothing is
registered, no buffer is opened, and the handler's exception is what surfaces. Every door
here used to carry on after the throw and call (or store) the callback anyway. A direct
`call_user_func('self::m')` compiled against the global function is a different face in
php (its specialised opcode wraps the exception as the previous of a TypeError), so it is
reached only through a variable and through an unqualified name in a namespace.
--FILE--
<?php
namespace CsdtNs;
class CsdtA {
    static function m(...$a) { echo "  callback ran\n"; return 1; }
    static function takes(callable $c) { echo "  body ran\n"; }
    function run() {
        $cuf = 'call_user_func';
        $doors = [
            'call_user_func by name' => fn() => $cuf('self::m'),
            'call_user_func unqualified' => fn() => call_user_func('self::m'),
            'array_map' => fn() => \array_map('self::m', [1]),
            'array_filter' => fn() => \array_filter([1], ['static', 'm']),
            'usort' => function () { $a = [2, 1]; \usort($a, 'self::m'); },
            'array_walk' => function () { $a = [1]; \array_walk($a, 'self::m'); },
            'preg_replace_callback' => fn() => \preg_replace_callback('/a/', 'self::m', 'a'),
            'iterator_apply' => fn() => \iterator_apply(new \ArrayIterator([1]), 'self::m'),
            'is_callable' => fn() => \var_dump(\is_callable('self::m')),
            'callable parameter' => fn() => self::takes('self::m'),
        ];
        \set_error_handler(function ($n, $s) { throw new \RuntimeException($s); });
        foreach ($doors as $k => $d) {
            echo "$k\n";
            try { $d(); echo "  no throw\n"; }
            catch (\Throwable $e) { echo "  ", \get_class($e), ": ", $e->getMessage(), "\n"; }
        }
        $n = \count(\spl_autoload_functions());
        try { \spl_autoload_register('self::m'); } catch (\RuntimeException $e) { echo "spl_autoload_register: ", $e->getMessage(), "\n"; }
        echo "  autoloaders added: ", \count(\spl_autoload_functions()) - $n, "\n";
        $l = \ob_get_level();
        try { \ob_start('self::m'); echo "  buffered\n"; } catch (\RuntimeException $e) { echo "ob_start: ", $e->getMessage(), "\n"; }
        echo "  buffers opened: ", \ob_get_level() - $l, "\n";
        \restore_error_handler();
    }
}
(new CsdtA)->run();
echo "done\n";
--EXPECT--
call_user_func by name
  RuntimeException: Use of "self" in callables is deprecated
call_user_func unqualified
  RuntimeException: Use of "self" in callables is deprecated
array_map
  RuntimeException: Use of "self" in callables is deprecated
array_filter
  RuntimeException: Use of "static" in callables is deprecated
usort
  RuntimeException: Use of "self" in callables is deprecated
array_walk
  RuntimeException: Use of "self" in callables is deprecated
preg_replace_callback
  RuntimeException: Use of "self" in callables is deprecated
iterator_apply
  RuntimeException: Use of "self" in callables is deprecated
is_callable
  RuntimeException: Use of "self" in callables is deprecated
callable parameter
  RuntimeException: Use of "self" in callables is deprecated
spl_autoload_register: Use of "self" in callables is deprecated
  autoloaders added: 0
ob_start: Use of "self" in callables is deprecated
  buffers opened: 0
done
