--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An error handler that throws on the callable-scope deprecation becomes the previous of a TypeError at three doors
--DESCRIPTION--
Closure::fromCallable(), a `callable` return type, and a call_user_func()/_array() php's
compiler binds to the global function (a literal name, no spread, no `name:` argument) each
refuse the callable with a TypeError of their own whose previous is the handler's exception.
That exception must not reach the caller's catch on its way out of the handler: the handler
runs behind a fence, and an outer finally, a catch body, a generator, a fiber and an uncaught
chain all see the TypeError. The same handler at the other shapes (a callable parameter, a
forward through a variable, a spread or a named argument) still surfaces bare, and a handler
that catches its own exception, or throws nothing, leaves the callable resolved.
--FILE--
<?php
namespace CsdwNs;
function csdw_handler($n, $s) { throw new \RuntimeException("H: $s"); }
class CsdwA {
    static function m(...$a) { echo "  m ran\n"; return 7; }
    static function r(): callable { return 'self::m'; }
    static function rn(): ?callable { return 'self::m'; }
    static function ru(): callable|int { return 'self::m'; }
    static function p(callable $c) { echo "  body ran\n"; }
    static function show($e) {
        echo "  ";
        do { echo \get_class($e), ": ", $e->getMessage(), " | "; } while ($e = $e->getPrevious());
        echo "\n";
    }
}
class CsdwB extends CsdwA {
    static function doors() {
        $cuf = '\call_user_func';
        $none = [];
        return [
            'fromCallable' => fn() => \Closure::fromCallable('self::m'),
            'call_user_func' => fn() => \call_user_func('self::m'),
            'call_user_func_array' => fn() => \call_user_func_array('parent::m', []),
            'return callable' => fn() => self::r(),
            'return ?callable' => fn() => self::rn(),
            'return callable|int' => fn() => self::ru(),
            'parameter' => fn() => self::p('self::m'),
            'call_user_func via variable' => fn() => $cuf('self::m'),
            'call_user_func unqualified' => fn() => call_user_func('self::m'),
            'call_user_func spread' => fn() => \call_user_func('self::m', ...$none),
            'call_user_func named' => fn() => \call_user_func(callback: 'self::m'),
            'call_user_func nested' => fn() => \call_user_func('call_user_func', 'self::m'),
        ];
    }
    static function run() {
        foreach (['throw', 'inner', 'quiet'] as $mode) {
            \set_error_handler(function ($n, $s) use ($mode) {
                if ($mode === 'throw') { throw new \RuntimeException("H: $s"); }
                if ($mode === 'inner') {
                    try { throw new \LogicException('in'); } catch (\LogicException $e) { echo "  handler caught its own\n"; }
                    return true;
                }
                echo "  handler: $s\n";
                return true;
            });
            echo "== $mode\n";
            foreach (self::doors() as $k => $d) {
                echo "$k\n";
                try { $d(); echo "  returned\n"; } catch (\Throwable $e) { self::show($e); }
            }
        }
        \set_error_handler(function ($n, $s) { throw new \RuntimeException("H: $s"); });
        echo "== around it\n";
        try {
            try { \Closure::fromCallable('self::m'); echo "  no\n"; } finally { echo "  finally ran\n"; }
        } catch (\Throwable $e) { self::show($e); }
        try { throw new \Exception('outer'); }
        catch (\Exception $x) {
            try { self::r(); } catch (\Throwable $e) { self::show($e); }
            echo "  catch body done\n";
        }
        $g = function () {
            try { yield \Closure::fromCallable('static::m'); }
            catch (\Throwable $e) { echo "  in generator:"; self::show($e); yield 1; }
        };
        foreach ($g() as $v) { echo "  generator yielded\n"; }
        $f = new \Fiber(function () {
            try { \Fiber::suspend(1); \call_user_func('self::m'); }
            catch (\Throwable $e) { echo "  in fiber:"; self::show($e); }
        });
        $f->start();
        $f->resume();
        echo "== uncaught\n";
        \set_error_handler('CsdwNs\\csdw_handler');
        \Closure::fromCallable('self::m');
    }
}
CsdwB::run();
--EXPECTF--
== throw
fromCallable
  TypeError: Failed to create closure from callable | RuntimeException: H: Use of "self" in callables is deprecated | 
call_user_func
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, (null) | RuntimeException: H: Use of "self" in callables is deprecated | 
call_user_func_array
  TypeError: call_user_func_array(): Argument #1 ($callback) must be a valid callback, (null) | RuntimeException: H: Use of "parent" in callables is deprecated | 
return callable
  TypeError: CsdwNs\CsdwA::r(): Return value must be of type callable, string returned | RuntimeException: H: Use of "self" in callables is deprecated | 
return ?callable
  TypeError: CsdwNs\CsdwA::rn(): Return value must be of type ?callable, string returned | RuntimeException: H: Use of "self" in callables is deprecated | 
return callable|int
  TypeError: CsdwNs\CsdwA::ru(): Return value must be of type callable|int, string returned | RuntimeException: H: Use of "self" in callables is deprecated | 
parameter
  RuntimeException: H: Use of "self" in callables is deprecated | 
call_user_func via variable
  RuntimeException: H: Use of "self" in callables is deprecated | 
call_user_func unqualified
  RuntimeException: H: Use of "self" in callables is deprecated | 
call_user_func spread
  RuntimeException: H: Use of "self" in callables is deprecated | 
call_user_func named
  RuntimeException: H: Use of "self" in callables is deprecated | 
call_user_func nested
  RuntimeException: H: Use of "self" in callables is deprecated | 
== inner
fromCallable
  handler caught its own
  returned
call_user_func
  handler caught its own
  m ran
  returned
call_user_func_array
  handler caught its own
  m ran
  returned
return callable
  handler caught its own
  returned
return ?callable
  handler caught its own
  returned
return callable|int
  handler caught its own
  returned
parameter
  handler caught its own
  body ran
  returned
call_user_func via variable
  handler caught its own
  m ran
  returned
call_user_func unqualified
  handler caught its own
  m ran
  returned
call_user_func spread
  handler caught its own
  m ran
  returned
call_user_func named
  handler caught its own
  m ran
  returned
call_user_func nested
  handler caught its own
  m ran
  returned
== quiet
fromCallable
  handler: Use of "self" in callables is deprecated
  returned
call_user_func
  handler: Use of "self" in callables is deprecated
  m ran
  returned
call_user_func_array
  handler: Use of "parent" in callables is deprecated
  m ran
  returned
return callable
  handler: Use of "self" in callables is deprecated
  returned
return ?callable
  handler: Use of "self" in callables is deprecated
  returned
return callable|int
  handler: Use of "self" in callables is deprecated
  returned
parameter
  handler: Use of "self" in callables is deprecated
  body ran
  returned
call_user_func via variable
  handler: Use of "self" in callables is deprecated
  m ran
  returned
call_user_func unqualified
  handler: Use of "self" in callables is deprecated
  m ran
  returned
call_user_func spread
  handler: Use of "self" in callables is deprecated
  m ran
  returned
call_user_func named
  handler: Use of "self" in callables is deprecated
  m ran
  returned
call_user_func nested
  handler: Use of "self" in callables is deprecated
  m ran
  returned
== around it
  finally ran
  TypeError: Failed to create closure from callable | RuntimeException: H: Use of "self" in callables is deprecated | 
  TypeError: CsdwNs\CsdwA::r(): Return value must be of type callable, string returned | RuntimeException: H: Use of "self" in callables is deprecated | 
  catch body done
  in generator:  TypeError: Failed to create closure from callable | RuntimeException: H: Use of "static" in callables is deprecated | 
  generator yielded
  in fiber:  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, (null) | RuntimeException: H: Use of "self" in callables is deprecated | 
== uncaught
%s Fatal error:  Uncaught RuntimeException: H: Use of "self" in callables is deprecated in %s
Stack trace:
%A
Next TypeError: Failed to create closure from callable in %s
Stack trace:
%A
  thrown in %s on line %d
