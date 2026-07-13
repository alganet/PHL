--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ucwords honors the $separators argument
--FILE--
<?php
// The optional second argument REPLACES the default whitespace separators.
echo ucwords("a-b c", "-"), "\n";        // A-B c  (only '-' is a boundary)
echo ucwords("foo|bar|baz", "|"), "\n";  // Foo|Bar|Baz
echo ucwords("hello-world foo", "- "), "\n"; // Hello-World Foo
// An empty separator string upper-cases only the very first character.
echo ucwords("hello world", ""), "\n";   // Hello world
// Default separators still apply when the argument is omitted.
echo ucwords("hello\tthere world"), "\n"; // Hello\tThere World
?>
--EXPECT--
A-B c
Foo|Bar|Baz
Hello-World Foo
Hello world
Hello	There World
--CLEAN--
<?php
