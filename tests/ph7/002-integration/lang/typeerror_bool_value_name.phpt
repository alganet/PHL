--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Engine typed-value TypeErrors name a bool as true/false and an object by class
--FILE--
<?php
// php renders the *value* name of the offending argument/return/property in a
// type TypeError: a bool prints `true`/`false` (not `bool`), an object prints
// its class name (not `object`). This covers the three engine sites that used
// to fall back to gettype-style names: parameter binding, the return-type
// check, and typed-property assignment.
function shortMsg(TypeError $e) {
    $msg = $e->getMessage();
    $pos = strpos($msg, ", called in");
    if ($pos !== false) $msg = substr($msg, 0, $pos);
    return $msg;
}

class C {}

// --- parameter binding (object hint rejects a bool / an array) ---
function takesObject(object $o) {}
try { takesObject(true);  } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { takesObject(false); } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { takesObject([1,2]); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// --- return-type check (array return given a bool) ---
function returnsArray(): array { return true; }
try { returnsArray(); } catch (TypeError $e) { echo shortMsg($e), "\n"; }
function returnsArrayF(): array { return false; }
try { returnsArrayF(); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// --- class-typed return given a bool ---
function returnsClass(): C { return true; }
try { returnsClass(); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// --- typed-property assignment (array property given a bool / an object) ---
class Box { public array $items; public int $n; }
$b = new Box();
try { $b->items = true;    } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { $b->items = false;   } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { $b->n     = new C(); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// --- Generator::throw with a non-Throwable bool argument ---
function gen() { yield 1; }
$g = gen();
$g->current();
try { $g->throw(true); } catch (TypeError $e) { echo shortMsg($e), "\n"; }
?>
--EXPECT--
takesObject(): Argument #1 ($o) must be of type object, true given
takesObject(): Argument #1 ($o) must be of type object, false given
takesObject(): Argument #1 ($o) must be of type object, array given
returnsArray(): Return value must be of type array, true returned
returnsArrayF(): Return value must be of type array, false returned
returnsClass(): Return value must be of type C, true returned
Cannot assign true to property Box::$items of type array
Cannot assign false to property Box::$items of type array
Cannot assign C to property Box::$n of type int
Generator::throw(): Argument #1 ($exception) must be of type Throwable, true given
--CLEAN--
<?php
