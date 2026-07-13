--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_word_count argument errors match php
--FILE--
<?php
class SwcErrStr { public function __toString(): string { return "b"; } }
class SwcErrPlain {}
try { str_word_count(); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { str_word_count([]); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { str_word_count("x", 5); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { str_word_count("x", -1); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { str_word_count("x", 0, []); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// non-numeric string format is a TypeError, not a silent 0
try { str_word_count("a b", "abc"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { str_word_count("a b", 0, new SwcErrPlain); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// __toString objects are accepted for $string and $characters
echo str_word_count(new SwcErrStr), "\n";
echo json_encode(str_word_count("a b abc", 1, new SwcErrStr)), "\n";
// numeric-string format is accepted in weak mode
echo json_encode(str_word_count("a b", "1")), "\n";
?>
--EXPECT--
ArgumentCountError: str_word_count() expects at least 1 argument, 0 given
TypeError: str_word_count(): Argument #1 ($string) must be of type string, array given
ValueError: str_word_count(): Argument #2 ($format) must be a valid format value
ValueError: str_word_count(): Argument #2 ($format) must be a valid format value
TypeError: str_word_count(): Argument #3 ($characters) must be of type ?string, array given
TypeError: str_word_count(): Argument #2 ($format) must be of type int, string given
TypeError: str_word_count(): Argument #3 ($characters) must be of type ?string, SwcErrPlain given
1
["a","b","abc"]
["a","b"]
--CLEAN--
<?php
