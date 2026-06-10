--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Code on the same line as a #[Attr] attribute is not swallowed
--DESCRIPTION--
Regression: the lexer treated '#' unconditionally as a line comment, so
'#[MyAttr] function f(){}' silently discarded the function declaration.
The balanced '#[ ... ]' group alone must be skipped, not the whole line.
--FILE--
<?php
#[MyAttr] function attr_same_line_f(){ return "ok"; }
echo attr_same_line_f(), "\n";
$attr_same_line_c = #[MyAttr] function(){ return "closure"; };
echo $attr_same_line_c(), "\n";
?>
--EXPECT--
ok
closure
--CLEAN--
<?php
?>
