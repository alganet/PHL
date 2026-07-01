--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
never return type: a throwing function runs; falling off the end throws a catchable TypeError (PHP 8.1)
--FILE--
<?php
function fail(string $m): never { throw new RuntimeException($m); }
try { fail("boom"); } catch (RuntimeException $e) { echo "fn: ", $e->getMessage(), "\n"; }

class NeverClass {
    public function m(): never { throw new LogicException("meth"); }
}
$o = new NeverClass();
try { $o->m(); } catch (LogicException $e) { echo "method: ", $e->getMessage(), "\n"; }

$c = function(): never { throw new Exception("clo"); };
try { $c(); } catch (Exception $e) { echo "closure: ", $e->getMessage(), "\n"; }

// Falling off the end without throwing/exiting is a runtime TypeError.
function neverImplicit(): never { $x = 1; }
try { neverImplicit(); } catch (TypeError $e) { echo "fell off: ", $e->getMessage(), "\n"; }

// A never method declared on an interface/abstract class is accepted.
interface NeverIface { public function f(): never; }
abstract class NeverAbstract { abstract public function g(): never; }
echo "declared ok\n";
?>
--EXPECT--
fn: boom
method: meth
closure: clo
fell off: neverImplicit(): never-returning function must not implicitly return
declared ok
--CLEAN--
<?php
