--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An autoloader a builtin triggered is traced under that builtin's frame, but keeps its caller's binding
--DESCRIPTION--
When class_exists(), is_a(), class_implements() or spl_autoload_call() asks the
autoloader for a class, php's trace shows the loader's frame with no file or line
and the builtin as a frame of its own at the userland call site -- the shape of any
callback an internal function reaches for. This engine gave the loader the call
site and dropped the builtin's frame. An opcode's own `new Foo` has no builtin to
name, so its loader keeps the line.

The autoload call is otherwise NOT a callback's: php binds its argument under the
strict_types of the code that named the class, and an argument diagnostic keeps its
`, called in FILE on line N` tail, whether a builtin triggered it or not. Both stay.
--FILE--
<?php
declare(strict_types=1);

function f(string $s): string { return str_replace(__FILE__, 'FILE', $s); }

function frames(): void {
    foreach (array_slice(debug_backtrace(), 1) as $f) {
        echo '  ', $f['class'] ?? '', $f['type'] ?? '', f($f['function']),
            ' line=', $f['line'] ?? '-', ' file=', isset($f['file']) ? 'yes' : 'no', "\n";
    }
}

spl_autoload_register(function ($c) { echo "load $c\n"; frames(); });

class_exists('A1');
interface_exists('A2');
is_a('A3', 'A4', true);
is_subclass_of('x', 'A5');
@class_implements('A6');
spl_autoload_call('A7');
function inner() { return enum_exists('A9'); }
inner();
$n = 'A10';
try { new $n; } catch (Error $e) {}

echo "-- thrown from the loader\n";
spl_autoload_register(function ($c) { throw new Exception("no $c"); }, true, true);
try { trait_exists('B1'); } catch (Exception $e) { echo f($e->getTraceAsString()), "\n"; }
try { new B2; } catch (Exception $e) { echo f($e->getTraceAsString()), "\n"; }
spl_autoload_unregister(spl_autoload_functions()[0]);

echo "-- binding stays the caller's\n";
spl_autoload_register(function (int $c) {}, true, true);
try { class_exists('C1'); } catch (TypeError $e) { echo f($e->getMessage()), "\n"; }
try { new C2; } catch (TypeError $e) { echo f($e->getMessage()), "\n"; }
spl_autoload_unregister(spl_autoload_functions()[0]);
spl_autoload_register(function ($a, $b) {}, true, true);
try { class_exists('C3'); } catch (ArgumentCountError $e) { echo f($e->getMessage()), "\n"; }
--EXPECT--
load A1
  {closure:FILE:13} line=- file=no
  class_exists line=15 file=yes
load A2
  {closure:FILE:13} line=- file=no
  interface_exists line=16 file=yes
load A3
  {closure:FILE:13} line=- file=no
  is_a line=17 file=yes
load x
  {closure:FILE:13} line=- file=no
  is_subclass_of line=18 file=yes
load A6
  {closure:FILE:13} line=- file=no
  class_implements line=19 file=yes
load A7
  {closure:FILE:13} line=- file=no
  spl_autoload_call line=20 file=yes
load A9
  {closure:FILE:13} line=- file=no
  enum_exists line=21 file=yes
  inner line=22 file=yes
load A10
  {closure:FILE:13} line=24 file=yes
-- thrown from the loader
#0 [internal function]: {closure:FILE:27}()
#1 FILE(28): trait_exists()
#2 {main}
#0 FILE(29): {closure:FILE:27}()
#1 {main}
-- binding stays the caller's
{closure:FILE:33}(): Argument #1 ($c) must be of type int, string given, called in FILE on line 34
{closure:FILE:33}(): Argument #1 ($c) must be of type int, string given, called in FILE on line 35
Too few arguments to function {closure:FILE:37}(), 1 passed in FILE on line 38 and exactly 2 expected
