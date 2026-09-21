--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Stringable object coerces to a string return type / string property (weak mode)
--FILE--
<?php
// php weak-mode coercion of an object with __toString() applies to a `string`
// RETURN type and a `string` typed PROPERTY, not just to parameters. Every
// other object->scalar case still throws, and typed-property stores are always
// weak so a Stringable object stores its __toString() there too.
function shortMsg(TypeError $e) { return $e->getMessage(); }

class Plain {}
class Str { function __toString(): string { return "hi"; } }

// --- string return type ---
function retString(object $o): string { return $o; }
var_dump(retString(new Str()));                 // coerces -> "hi"
try { retString(new Plain()); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// --- object with __toString to a non-string return still throws ---
function retInt(object $o): int { return $o; }
try { retInt(new Str()); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// --- string typed property (stores are always weak mode) ---
class Box { public string $s; public int $n; }
$b = new Box();
$b->s = new Str();                              // coerces -> "hi"
var_dump($b->s);
try { $b->s = new Plain(); } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { $b->n = new Str();   } catch (TypeError $e) { echo shortMsg($e), "\n"; }
?>
--EXPECT--
string(2) "hi"
retString(): Return value must be of type string, Plain returned
retInt(): Return value must be of type int, Str returned
string(2) "hi"
Cannot assign Plain to property Box::$s of type string
Cannot assign Str to property Box::$n of type int
--CLEAN--
<?php
