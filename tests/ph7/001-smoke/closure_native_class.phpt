--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure is a C class: no properties, a private ctor, and call() declares its own
--DESCRIPTION--
Closure's three methods were C already; its DECLARATION was still embedded PHP,
which cost three php-visible things. The engine slots $__fn/$__this/$__scope were
on every presentation surface (php's Closure has NO property, and a `__`-prefixed
one is exactly what the native-class workstream's ledger counts); __construct was
public where php's is private, so Reflection printed the wrong modifier; and
call() was PHP that reached get_class(), whose TypeError named ITS parameter
rather than call()'s. php refuses `new Closure` in its create_object handler,
before the constructor's visibility is consulted, which is why the message names
the instantiation even though the ctor is private.
--FILE--
<?php
$clnFn = function ($x) { return $x * 2; };

/* php presents no property for a Closure. */
echo 'props => ', count((new ReflectionClass('Closure'))->getProperties()), "\n";
echo 'object vars => ', count(get_object_vars($clnFn)), "\n";
$clnN = 0;
foreach ($clnFn as $clnK => $clnV) { $clnN++; }
echo 'foreach => ', $clnN, "\n";

/* Declaration: final, a PRIVATE constructor, and a refusal that names the
 * instantiation rather than the visibility. */
echo 'final => ', var_export((new ReflectionClass('Closure'))->isFinal(), true), "\n";
echo 'ctor private => ', var_export((new ReflectionMethod('Closure', '__construct'))->isPrivate(), true), "\n";
try { new Closure(); } catch (Throwable $e) { echo 'new => ', get_class($e), ': ', $e->getMessage(), "\n"; }
try { (new ReflectionClass('Closure'))->newInstance(); }
catch (Throwable $e) { echo 'newInstance => ', get_class($e), ': ', $e->getMessage(), "\n"; }
try { serialize($clnFn); } catch (Throwable $e) { echo 'serialize => ', get_class($e), ': ', $e->getMessage(), "\n"; }

/* call(): binds $this AND the scope of the new $this's class. */
class ClnBox { private $v = 7; }
$clnRead = function () { return $this->v; };
echo 'call => ', $clnRead->call(new ClnBox), "\n";
$clnSum = function (...$a) { return array_sum($a) + $this->v; };
echo 'call args => ', $clnSum->call(new ClnBox, 1, 2), "\n";
try { $clnRead->call('x'); } catch (Throwable $e) { echo 'call type => ', get_class($e), ': ', $e->getMessage(), "\n"; }
try { $clnRead->call(); } catch (Throwable $e) { echo 'call arity => ', get_class($e), ': ', $e->getMessage(), "\n"; }

/* The declared signature is what Reflection reports, defaults included: php
 * prints an INTERNAL parameter's string default DOUBLE-quoted, unlike a
 * userland one. */
function clnParamLine($export, $n) {
    foreach (explode("\n", $export) as $line) {
        $line = trim($line);
        if (strncmp($line, 'Parameter #' . $n . ' ', 12) === 0) { return $line; }
    }
    return '<missing>';
}
echo 'call sig => ', clnParamLine((string)new ReflectionMethod('Closure', 'call'), 0), "\n";
echo 'call ret => ', (string)(new ReflectionMethod('Closure', 'call'))->getReturnType(), "\n";
echo 'bindTo default => ', clnParamLine((string)new ReflectionMethod('Closure', 'bindTo'), 1), "\n";
echo 'internal quoting => ', clnParamLine((string)new ReflectionFunction('str_getcsv'), 2), "\n";
function clnUser($a = 'abc') {}
echo 'user quoting => ', clnParamLine((string)new ReflectionFunction('clnUser'), 0), "\n";

/* bindTo/bind/fromCallable still work through the native declaration. */
$clnBound = $clnRead->bindTo(new ClnBox, ClnBox::class);
echo 'bindTo => ', $clnBound(), "\n";
echo 'bind => ', Closure::bind($clnRead, new ClnBox, ClnBox::class)(), "\n";
echo 'fromCallable => ', Closure::fromCallable('strtoupper')('ok'), "\n";
echo 'clone => ', (clone $clnFn)(4), "\n";
--EXPECT--
props => 0
object vars => 0
foreach => 0
final => true
ctor private => true
new => Error: Instantiation of class Closure is not allowed
newInstance => Error: Instantiation of class Closure is not allowed
serialize => Exception: Serialization of 'Closure' is not allowed
call => 7
call args => 10
call type => TypeError: Closure::call(): Argument #1 ($newThis) must be of type object, string given
call arity => ArgumentCountError: Closure::call() expects at least 1 argument, 0 given
call sig => Parameter #0 [ <required> object $newThis ]
call ret => mixed
bindTo default => Parameter #1 [ <optional> object|string|null $newScope = "static" ]
internal quoting => Parameter #2 [ <optional> string $enclosure = "\"" ]
user quoting => Parameter #0 [ <optional> $a = 'abc' ]
bindTo => 7
bind => 7
fromCallable => OK
clone => 8
