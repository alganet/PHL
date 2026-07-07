--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An explicit null argument is not replaced by a parameter's default value
--DESCRIPTION--
PHP applies a default only for an OMITTED argument, never for an explicit null.
An explicit null therefore reaches the type check: it stays null for a typeless
param, throws a TypeError for a non-nullable typed param, and is accepted for an
explicit `?Type` nullable param. (The implicit `Type $x = null` nullable form
behaves the same in PHL but is left out of this cross-engine test — php emits an
8.4 deprecation notice for it that PHL does not.) The ", called in <file> on
line N" tail PHP appends to the TypeError is intentionally not asserted (PHL
omits it for every exception — a separate error-format-fidelity item).
--FILE--
<?php
function endTe(callable $fn): string {
    try { $fn(); return "no-throw"; }
    catch (\TypeError $e) {
        return str_contains($e->getMessage(), "must be of type") ? "TE" : "TE?";
    }
}

/* typeless param with a default: an explicit null stays null, an omitted arg defaults */
function endTypeless($x = 5) { return var_export($x, true); }
echo "typeless-null:", endTypeless(null), "\n";   // NULL (not 5)
echo "typeless-omit:", endTypeless(), "\n";       // 5

/* non-nullable typed param with a default: an explicit null throws */
function endTyped(int $x = 5) { return $x; }
echo "typed-null:", endTe(fn() => endTyped(null)), "\n";  // TE
echo "typed-omit:", endTyped(), "\n";                     // 5

/* the message prefix is PHP-exact (sans the "called in" suffix) */
try { endTyped(null); } catch (\TypeError $e) {
    echo substr($e->getMessage(), 0, strpos($e->getMessage(), " given") + 6), "\n";
}

/* an explicit `?type` default accepts null (the implicitly-nullable
 * `Type $x = null` form behaves the same in PHL but carries an 8.4 deprecation
 * notice under php, so it is left out of this cross-engine test) */
function endNullable(?int $x = null) { return var_export($x, true); }
echo "explicit-nullable:", endNullable(null), "\n";  // NULL

/* the named-argument binding path behaves identically */
function endNamedTypeless($p = 1, $x = 5)      { return var_export($x, true); }
function endNamedTyped($p = 1, int $x = 5)     { return $x; }
echo "named-typeless:", endNamedTypeless(x: null), "\n";           // NULL
echo "named-typed:", endTe(fn() => endNamedTyped(x: null)), "\n";  // TE
?>
--EXPECT--
typeless-null:NULL
typeless-omit:5
typed-null:TE
typed-omit:5
endTyped(): Argument #1 ($x) must be of type int, null given
explicit-nullable:NULL
named-typeless:NULL
named-typed:TE
