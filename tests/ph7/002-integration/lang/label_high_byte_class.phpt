--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A php LABEL is [a-zA-Z_\x80-\xff][a-zA-Z0-9_\x80-\xff]*: every byte >= 0x80 names things, not just a UTF-8 lead byte (PHL required >= 0xc0, so a 0x80-0xbf byte was a parse error or literal text)
--FILE--
<?php
// The names below carry a \x80 byte, which php accepts in a label but which can
// never LEAD a valid UTF-8 sequence -- the byte class PHL's scanners were missing.
// Written through eval() so the file itself stays valid UTF-8 for editors and
// diff tools; the lexer sees exactly the same bytes either way.
$b = "\x80";

eval("\$$b" . "v = 4; \$GLOBALS['out'][] = \$$b" . "v;");                 // variable
eval("\$$b" . "v = 4; \$GLOBALS['out'][] = \"\$$b" . "v\";");             // interpolated
eval("\$$b" . "v = 4; \$f = fn() => \$$b" . "v * 2; \$GLOBALS['out'][] = \$f();"); // arrow capture
eval("function $b" . "fn() { return 'fn'; } \$GLOBALS['out'][] = $b" . "fn();");   // function
eval("class $b" . "C { const $b" . "K = 'K'; public \$$b" . "p = 'p';"
   . " function $b" . "m() { return 'm'; } }"
   . " \$o = new $b" . "C;"
   . " \$GLOBALS['out'][] = $b" . "C::$b" . "K . \$o->$b" . "p . \$o->$b" . "m();"); // class members
eval("\$o = new stdClass; \$o->$b" . "p = 'prop'; \$GLOBALS['out'][] = \"\$o->$b" . "p\";"); // "->name"
eval("\$a = ['$b" . "k' => 'sub']; \$GLOBALS['out'][] = \"\$a[$b" . "k]\";");      // "$a[name]"
eval("\$x = 'X'; \$GLOBALS['out'][] = <<<$b" . "EOT\n  got \$x\n$b" . "EOT;");     // heredoc label

// A well-formed multibyte name keeps working -- it is the same rule, since every
// byte of a UTF-8 sequence is >= 0x80.
$café = 'utf8';
$out[] = "$café";

echo implode('|', $out), "\n";
?>
--EXPECT--
4|4|8|fn|Kpm|prop|sub|  got X|utf8
