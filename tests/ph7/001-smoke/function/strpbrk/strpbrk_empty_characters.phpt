--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strpbrk() rejects an empty $characters instead of answering a bare false
--FILE--
<?php
// An empty set can never match, so php rejects it rather than returning the
// FALSE that also means "not found". PH7 answered false for both.
try {
    strpbrk("abc", "");
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
// Checked before the haystack, so an empty subject still throws.
try {
    strpbrk("", "");
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
// A non-empty set that simply does not occur keeps answering false.
var_dump(strpbrk("", "z"));
var_dump(strpbrk("abc", "z"));
var_dump(strpbrk("This is a test", "st"));
?>
--EXPECT--
strpbrk(): Argument #2 ($characters) must be a non-empty string
strpbrk(): Argument #2 ($characters) must be a non-empty string
bool(false)
bool(false)
string(11) "s is a test"
--CLEAN--
<?php
