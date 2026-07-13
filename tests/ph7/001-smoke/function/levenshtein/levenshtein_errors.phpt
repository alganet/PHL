--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
levenshtein argument errors match php
--FILE--
<?php
class LevErrStr { public function __toString(): string { return "kitten"; } }
class LevErrPlain {}
try { levenshtein("a"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { levenshtein([], "b"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { levenshtein("a", "b", "x"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { levenshtein("a", "b", 2, [], 1); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// a numeric PREFIX is not a numeric string
try { levenshtein("a", "b", "10abc"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// NAN/INF and out-of-range floats fail outright
try { levenshtein("a", "b", NAN); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { levenshtein("a", "b", 1e20); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// objects: TypeError names the class, __toString objects are accepted as strings
try { levenshtein("a", "b", new LevErrPlain); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
echo levenshtein(new LevErrStr, "sitting"), "\n";
// float subjects are stringified in weak mode
echo levenshtein("1.5", 1.5), "\n";
?>
--EXPECT--
ArgumentCountError: levenshtein() expects at least 2 arguments, 1 given
TypeError: levenshtein(): Argument #1 ($string1) must be of type string, array given
TypeError: levenshtein(): Argument #3 ($insertion_cost) must be of type int, string given
TypeError: levenshtein(): Argument #4 ($replacement_cost) must be of type int, array given
TypeError: levenshtein(): Argument #3 ($insertion_cost) must be of type int, string given
TypeError: levenshtein(): Argument #3 ($insertion_cost) must be of type int, float given
TypeError: levenshtein(): Argument #3 ($insertion_cost) must be of type int, float given
TypeError: levenshtein(): Argument #3 ($insertion_cost) must be of type int, LevErrPlain given
3
0
--CLEAN--
<?php
