--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Non-nullable param rejects explicit null and non-numeric string (TypeError)
--FILE--
<?php
class C {}
class D {}

function needsC(C $c) { echo "C ok\n"; }
function needsInt(int $x) { echo "int:$x\n"; }
function needsFloat(float $x) { echo "float:$x\n"; }

// Explicit null to a non-nullable class param -> TypeError
try { needsC(null); } catch (TypeError $e) { echo "null->C TypeError\n"; }
// Wrong class -> TypeError
try { needsC(new D()); } catch (TypeError $e) { echo "D->C TypeError\n"; }
// Valid instance passes
needsC(new C());

// Explicit null to a non-nullable scalar param -> TypeError
try { needsInt(null); } catch (TypeError $e) { echo "null->int TypeError\n"; }
// Fully non-numeric string -> TypeError
try { needsInt("abc"); } catch (TypeError $e) { echo "abc->int TypeError\n"; }
// Leading-numeric-with-garbage string is NOT accepted (PHP 8) -> TypeError
try { needsInt("12abc"); } catch (TypeError $e) { echo "12abc->int TypeError\n"; }
// Empty string -> TypeError
try { needsInt(""); } catch (TypeError $e) { echo "empty->int TypeError\n"; }
try { needsFloat("xyz"); } catch (TypeError $e) { echo "xyz->float TypeError\n"; }
// Dangling exponent is not a numeric string -> TypeError
try { needsInt("1e"); } catch (TypeError $e) { echo "1e->int TypeError\n"; }
// A lone dot is not numeric -> TypeError
try { needsFloat("."); } catch (TypeError $e) { echo ".->float TypeError\n"; }

// Valid weak-mode coercions still work
needsInt("42");     // numeric string
needsInt("  42  "); // surrounding whitespace allowed
needsInt(true);     // bool -> int
needsFloat("1e3");  // numeric float-string
needsFloat(".5");   // leading-decimal numeric string
needsFloat("-.5");  // signed leading-decimal
needsFloat("5.");   // trailing-dot numeric string
?>
--EXPECT--
null->C TypeError
D->C TypeError
C ok
null->int TypeError
abc->int TypeError
12abc->int TypeError
empty->int TypeError
xyz->float TypeError
1e->int TypeError
.->float TypeError
int:42
int:42
int:1
float:1000
float:0.5
float:-0.5
float:5
--CLEAN--
<?php
