--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_replace error cases match php
--FILE--
<?php
class SbrErrStr { public function __toString(): string { return "xy"; } }
class SbrErrPlain {}
try { substr_replace("a"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { substr_replace("a", "b"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { substr_replace("hello", "X", [1], 1); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { substr_replace("hello", "X", 1, [1]); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// php validates all args ZPP-style before the body runs
try { substr_replace(new SbrErrPlain, "X", 1); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { substr_replace("ab", new SbrErrPlain, 1); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { substr_replace(["ab"], new SbrErrPlain, 1); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { substr_replace("hello", "X", "zz", 1); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { substr_replace("hello", "X", 1, "zz"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// __toString objects resolve for $string and $replace
echo substr_replace(new SbrErrStr, "R", 1), "\n";
echo substr_replace("ab", new SbrErrStr, 1), "\n";
?>
--EXPECT--
ArgumentCountError: substr_replace() expects at least 3 arguments, 1 given
ArgumentCountError: substr_replace() expects at least 3 arguments, 2 given
TypeError: substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string
TypeError: substr_replace(): Argument #4 ($length) cannot be an array when working on a single string
TypeError: substr_replace(): Argument #1 ($string) must be of type array|string, SbrErrPlain given
TypeError: substr_replace(): Argument #2 ($replace) must be of type array|string, SbrErrPlain given
TypeError: substr_replace(): Argument #2 ($replace) must be of type array|string, SbrErrPlain given
TypeError: substr_replace(): Argument #3 ($offset) must be of type array|int, string given
TypeError: substr_replace(): Argument #4 ($length) must be of type array|int|null, string given
xR
axy
--CLEAN--
<?php
