--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed scalar parameter rejects an object; only __toString->string coerces
--FILE--
<?php
// php's ONLY object->scalar weak coercion for a typed parameter is an object
// with __toString() passed to a `string` parameter. Every other target
// (int/float/bool) throws, as does a `string` target on a non-stringable
// object. PHL used to silently produce int(1)/float(1)/bool(true)/"Object".
function shortMsg(TypeError $e) {
    $msg = $e->getMessage();
    $pos = strpos($msg, ", called in");
    if ($pos !== false) $msg = substr($msg, 0, $pos);
    return $msg;
}

class Plain {}
class Str { function __toString(): string { return "hi"; } }

function wantsInt(int $x)       { var_dump($x); }
function wantsFloat(float $x)   { var_dump($x); }
function wantsBool(bool $x)     { var_dump($x); }
function wantsString(string $x) { var_dump($x); }

// Non-stringable object to any scalar -> TypeError
try { wantsInt(new Plain());    } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsFloat(new Plain());  } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsBool(new Plain());   } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsString(new Plain()); } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// Stringable object: coerces to string, but NOT to int/float/bool
try { wantsInt(new Str());   } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { wantsBool(new Str());  } catch (TypeError $e) { echo shortMsg($e), "\n"; }
wantsString(new Str());          // the one accepted case -> "hi"
?>
--EXPECT--
wantsInt(): Argument #1 ($x) must be of type int, Plain given
wantsFloat(): Argument #1 ($x) must be of type float, Plain given
wantsBool(): Argument #1 ($x) must be of type bool, Plain given
wantsString(): Argument #1 ($x) must be of type string, Plain given
wantsInt(): Argument #1 ($x) must be of type int, Str given
wantsBool(): Argument #1 ($x) must be of type bool, Str given
string(2) "hi"
--CLEAN--
<?php
