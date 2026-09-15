--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strrpos offset handling: range errors, negative offsets and the empty needle
--FILE--
<?php
// The last occurrence is found even when it sits at position 0.
var_dump(strrpos("abc", "a"));
var_dump(strrpos("abcabc", "a"));

// A non-negative offset is a LOWER bound on the match position.
var_dump(strrpos("aXbXc", "X", 2));
// A negative offset is an UPPER bound counted back from the end.
var_dump(strrpos("aXbXc", "X", -2));

// The empty needle matches at the highest position the offset allows.
var_dump(strrpos("hello", ""));
var_dump(strrpos("hello", "", 2));
var_dump(strrpos("hello", "", -2));

// |offset| may equal the haystack length ...
var_dump(strrpos("Hello World", "", 11));
var_dump(strrpos("Hello World", "", -11));
// ... but anything beyond it is a ValueError, not a false return.
try { strrpos("Hello World", "World", 100); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { strrpos("Hello World", "World", -100); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }

// A needle longer than the haystack simply is not found.
var_dump(strrpos("ab", "abc"));
?>
--EXPECT--
int(0)
int(3)
int(3)
int(3)
int(5)
int(5)
int(3)
int(11)
int(0)
strrpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
strrpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
bool(false)
--CLEAN--
<?php
