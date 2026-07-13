--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
similar_text argument errors match php
--FILE--
<?php
class SimErrStr { public function __toString(): string { return "kitten"; } }
class SimErrPlain {}
try { similar_text("a"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { similar_text([], ""); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// objects: TypeError names the class, __toString objects are accepted
try { similar_text("ab", new SimErrPlain); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
echo similar_text(new SimErrStr, "kitten"), "\n";
// ints are stringified in weak mode
echo similar_text(1122, 21), "\n";
?>
--EXPECT--
ArgumentCountError: similar_text() expects at least 2 arguments, 1 given
TypeError: similar_text(): Argument #1 ($string1) must be of type string, array given
TypeError: similar_text(): Argument #2 ($string2) must be of type string, SimErrPlain given
6
1
--CLEAN--
<?php
