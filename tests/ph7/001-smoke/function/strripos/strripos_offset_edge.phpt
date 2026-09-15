--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strripos offset handling: range errors, negative offsets and the empty needle
--FILE--
<?php
// The last occurrence is found even when it sits at position 0.
var_dump(strripos("abc", "A"));
var_dump(strripos("abcabc", "A"));

// A non-negative offset is a LOWER bound on the match position.
var_dump(strripos("aXbXc", "x", 2));
// A negative offset is an UPPER bound counted back from the end.
var_dump(strripos("aXbXc", "x", -2));

// The empty needle matches at the highest position the offset allows.
var_dump(strripos("hello", ""));
var_dump(strripos("hello", "", 2));
var_dump(strripos("hello", "", -2));

// |offset| may equal the haystack length ...
var_dump(strripos("Hello World", "", 11));
var_dump(strripos("Hello World", "", -11));
// ... but anything beyond it is a ValueError, not a false return.
try { strripos("Hello World", "World", 100); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { strripos("Hello World", "World", -100); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }

// A needle longer than the haystack simply is not found.
var_dump(strripos("ab", "abc"));
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
strripos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
strripos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
bool(false)
--CLEAN--
<?php
