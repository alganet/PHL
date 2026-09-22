--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class constant whose initializer FAILS re-evaluates at every access
--DESCRIPTION--
php evaluates a class constant's expression when the constant is read, and
keeps no failed result: `class C { const K = UNDEF; }` is silent at the
declaration and raises `Undefined constant "UNDEF"` on EVERY read of C::K --
directly, through constant(), through reflection, and through a static default
that names it. PHL memoized the slot before the throw ("so re-access doesn't
loop"), so only the FIRST read raised and every later one answered NULL in
silence; the deferred static default that named the constant then found it
materialized and raised nothing at all. A TYPED constant gets the same
treatment: php validates one at the declaration only when the initializer can
be evaluated there (`const int K = "x"` / `= PHP_EOL` stay declaration-time
fatals), and when the value only arrives at the access it reports the
CATCHABLE `TypeError: Cannot assign string to class constant C::K of type int`
instead -- also on every access.
--FILE--
<?php
echo "== never read: silent ==\n";
class CfeNever { const K = CFE_UNDEF_A; }
echo "declared\n";

echo "== every read raises, not just the first ==\n";
class CfeRead { const K = CFE_UNDEF_B; }
for ($i = 1; $i <= 3; $i++) {
    try { var_dump(CfeRead::K); } catch (Error $e) { echo $i, ": ", $e->getMessage(), "\n"; }
}

echo "== a typed constant is silent at the declaration too ==\n";
class CfeTyped { const int K = CFE_UNDEF_C; }
echo "declared\n";
try { var_dump(CfeTyped::K); } catch (Error $e) { echo "1: ", get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump(CfeTyped::K); } catch (Error $e) { echo "2: ", get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== through constant() and reflection too ==\n";
class CfeStr { const K = CFE_UNDEF_D; }
try { var_dump(constant('CfeStr::K')); } catch (Error $e) { echo "constant(): ", $e->getMessage(), "\n"; }
try { var_dump((new ReflectionClass('CfeStr'))->getConstant('K')); } catch (Error $e) { echo "reflection: ", $e->getMessage(), "\n"; }

echo "== the retry sees a later define() ==\n";
class CfeLate { const K = CFE_LATE; }
define('CFE_LATE', 7);
var_dump(CfeLate::K, CfeLate::K);

echo "== a value that only arrives at the access is still type-checked ==\n";
class CfeLateTyped { const int K = CFE_LATE_S; }
define('CFE_LATE_S', "no");
try { var_dump(CfeLateTyped::K); } catch (Throwable $e) { echo "1: ", get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump(CfeLateTyped::K); } catch (Throwable $e) { echo "2: ", get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== a static default naming a failed constant raises at its own access ==\n";
class CfeHost { const K = CFE_UNDEF_E; public static $s = self::K; }
try { var_dump(CfeHost::K); } catch (Error $e) { echo "const: ", $e->getMessage(), "\n"; }
try { var_dump(CfeHost::$s); } catch (Error $e) { echo "static: ", $e->getMessage(), "\n"; }
echo "end\n";
?>
--EXPECT--
== never read: silent ==
declared
== every read raises, not just the first ==
1: Undefined constant "CFE_UNDEF_B"
2: Undefined constant "CFE_UNDEF_B"
3: Undefined constant "CFE_UNDEF_B"
== a typed constant is silent at the declaration too ==
declared
1: Error: Undefined constant "CFE_UNDEF_C"
2: Error: Undefined constant "CFE_UNDEF_C"
== through constant() and reflection too ==
constant(): Undefined constant "CFE_UNDEF_D"
reflection: Undefined constant "CFE_UNDEF_D"
== the retry sees a later define() ==
int(7)
int(7)
== a value that only arrives at the access is still type-checked ==
1: TypeError: Cannot assign string to class constant CfeLateTyped::K of type int
2: TypeError: Cannot assign string to class constant CfeLateTyped::K of type int
== a static default naming a failed constant raises at its own access ==
const: Undefined constant "CFE_UNDEF_E"
static: Undefined constant "CFE_UNDEF_E"
end
--CLEAN--
<?php
