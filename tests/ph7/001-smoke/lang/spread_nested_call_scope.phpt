--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Argument unpacking scopes per call: a nested spread call keeps the outer's count
--FILE--
<?php
/* A spread-bearing call NESTED in another call's argument list — including as a
 * named argument's value — must not disturb the outer call's expansion. PHL used
 * a single shared spread accumulator that the inner call both over-read (it saw
 * the outer's pending expansion too) and then zeroed (the outer lost its
 * elements). Each call now derives its OWN expansion from the captured runs
 * (VmSpreadOwnExtra), scoped by the run boundary rather than an ambiguous slot
 * address. Verified byte-identical to php 8.5.7. */

function snscI(...$xs) { return array_sum($xs); }        // inner, itself spreads
function snscV(...$xs) { return count($xs) . "/" . array_sum($xs); }
function snscH($a, $b, ...$rest) { return "$a|$b|" . count($rest); }

// The canonical case: nested spread call as a named argument's value.
echo snscH(1, 2, ...range(3, 8), z: snscI(...range(1, 30))), "\n";   // 1|2|7
// Inner spread call as a positional-callee whose own count must be right.
$g = function (...$xs) { return count($xs); };
echo $g(...[1, 2, 3], k: snscI(...[4, 5])), "\n";                    // 4

// Empty outer spread (...[]) whose zero-width run shares the nested call's base
// slot — must not be consumed by the inner call (used to hang, then wrong-count).
echo snscV(...[], k: snscI(...[1, 2, 3])), "\n";                     // 1/6
echo snscV(...[], ...[], k: 5), "\n";                               // 1/5
echo snscV(...[], ...(snscI(...[1, 2]) ? [7, 8, 9] : [])), "\n";     // 3/24

// Methods / static / constructor: the target slot must not offset the count.
class SnscC {
    public $n;
    public function __construct(...$xs) { $this->n = count($xs); }
    public function m(...$xs) { return count($xs); }
    public static function s(...$xs) { return count($xs); }
}
echo (new SnscC())->m(...[1, 2], y: snscI(...[3, 4, 5])), "\n";      // 3
echo SnscC::s(...[1, 2], y: snscI(...[3, 4])), "\n";                 // 3
echo (new SnscC(...[1, 2, 3], x: snscI(...[4, 5])))->n, "\n";        // 4
echo (new SnscC())->m(...[], k: snscI(...[9, 9])), "\n";            // 1

// Deeper nesting: three live spread levels at once.
echo snscV(...[1, 2], k: snscI(...[3, 4], j: snscI(...[5, 6, 7]))), "\n"; // 3/28

// The inner result feeds the outer positionally (evaluated first, no overlap).
echo snscV(snscI(...range(1, 5)), ...range(1, 4)), "\n";            // 5/25
?>
--EXPECT--
1|2|7
4
1/6
1/5
3/24
3
3
4
1
3/28
5/25
--CLEAN--
<?php
