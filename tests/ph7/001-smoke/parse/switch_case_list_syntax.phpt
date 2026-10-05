--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A malformed switch case list is a parse error, and a template switch spans its PHP blocks
--FILE--
<?php
// A switch's case list holds `case`, `default` and the closing word or brace,
// after at most ONE leading `;`. Anything else is php's parse error naming
// the token and what the list wanted, and so is a broken head or an input
// that ends inside the list.
$cases = [
    'switch (1) { echo 1; }',
    'switch (1) { $x = 1; }',
    'switch (1) { 1; }',
    'switch (1) { "a"; }',
    'switch (1) { foo(); }',
    'switch (1) { if (1) {} }',
    'switch (1) { function scls_f() {} }',
    "switch (1) {\n\n  echo 1;\n}",
    'switch (1) { ;; case 1: }',
    'switch (1): echo 1; endswitch;',
    'switch (1): ;; case 1: endswitch;',
    'switch (1) { endswitch; }',
    'switch (1) { case 1: echo 1; endswitch; }',
    'function scls_g() { switch (1): case 1: }',
    'switch (1): case 1: }',
    'switch (1): }',
    'switch (1) echo 1;',
    'switch (1) ;',
    'switch (1)',
    "switch (1)\n\n",
    'switch;',
    'switch',
    'switch 1 { }',
    'switch () { }',
    'switch (1) {',
    "switch (1) {\ncase 1:\n  echo 1;\n",
    'switch (1):',
    "switch (1): case 1:\n",
    'switch (1): ?>  <?php case 1: endswitch;',
    "switch (1): ?>\nhello<?php case 1: endswitch;",
    'switch (1): ; ?><?php case 1: endswitch;',
    "switch (1) { case 1: ?>A\n",
    'if (1) {',
    "while (1) {\n\n",
];
foreach ($cases as $code) {
    try {
        eval($code);
        echo "compiled: ", json_encode($code), "\n";
    } catch (ParseError $e) {
        echo $e->getMessage(), ' (line ', $e->getLine(), ') -- ', json_encode($code), "\n";
    }
}

// The one leading `;` is php's to take, and a closing tag is one: a template's
// switch runs on across its PHP blocks, the text between them included.
eval('switch (2) { ; case 1: echo "one"; break; case 2: echo "two"; } echo "\n";');
eval('switch (2): ; case 2: echo "two"; endswitch; echo "\n";');
eval('switch (2) { ?>
<?php case 1: ?>one<?php break; case 2: ?>two<?php break; default: ?>other<?php } echo "\n";');
eval('switch (3): ?>
<?php case 1: ?>one<?php break; ?><?php default: ?>[<?= "other" ?>]<?php endswitch; echo "\n";');
eval('switch (1) { case 1: ?><?php // note ?><?php echo "after a comment-only block"; } echo "\n";');
eval('switch (1) {} echo "empty\n";');
eval('switch (1): endswitch; echo "empty alt\n";');
--EXPECT--
syntax error, unexpected token "echo", expecting "case" or "default" or "}" (line 1) -- "switch (1) { echo 1; }"
syntax error, unexpected variable "$x", expecting "case" or "default" or "}" (line 1) -- "switch (1) { $x = 1; }"
syntax error, unexpected integer "1", expecting "case" or "default" or "}" (line 1) -- "switch (1) { 1; }"
syntax error, unexpected double-quoted string "a", expecting "case" or "default" or "}" (line 1) -- "switch (1) { \"a\"; }"
syntax error, unexpected identifier "foo", expecting "case" or "default" or "}" (line 1) -- "switch (1) { foo(); }"
syntax error, unexpected token "if", expecting "case" or "default" or "}" (line 1) -- "switch (1) { if (1) {} }"
syntax error, unexpected token "function", expecting "case" or "default" or "}" (line 1) -- "switch (1) { function scls_f() {} }"
syntax error, unexpected token "echo", expecting "case" or "default" or "}" (line 3) -- "switch (1) {\n\n  echo 1;\n}"
syntax error, unexpected token ";", expecting "case" or "default" or "}" (line 1) -- "switch (1) { ;; case 1: }"
syntax error, unexpected token "echo", expecting "endswitch" or "case" or "default" (line 1) -- "switch (1): echo 1; endswitch;"
syntax error, unexpected token ";", expecting "endswitch" or "case" or "default" (line 1) -- "switch (1): ;; case 1: endswitch;"
syntax error, unexpected token "endswitch", expecting "case" or "default" or "}" (line 1) -- "switch (1) { endswitch; }"
syntax error, unexpected token "endswitch", expecting "case" or "default" or "}" (line 1) -- "switch (1) { case 1: echo 1; endswitch; }"
syntax error, unexpected token "}", expecting "endswitch" or "case" or "default" (line 1) -- "function scls_g() { switch (1): case 1: }"
Unmatched '}' (line 1) -- "switch (1): case 1: }"
Unmatched '}' (line 1) -- "switch (1): }"
syntax error, unexpected token "echo", expecting ":" or "{" (line 1) -- "switch (1) echo 1;"
syntax error, unexpected token ";", expecting ":" or "{" (line 1) -- "switch (1) ;"
syntax error, unexpected end of file, expecting ":" or "{" (line 1) -- "switch (1)"
syntax error, unexpected end of file, expecting ":" or "{" (line 3) -- "switch (1)\n\n"
syntax error, unexpected token ";", expecting "(" (line 1) -- "switch;"
syntax error, unexpected end of file, expecting "(" (line 1) -- "switch"
syntax error, unexpected integer "1", expecting "(" (line 1) -- "switch 1 { }"
syntax error, unexpected token ")" (line 1) -- "switch () { }"
Unclosed '{' (line 1) -- "switch (1) {"
Unclosed '{' on line 1 (line 4) -- "switch (1) {\ncase 1:\n  echo 1;\n"
syntax error, unexpected end of file, expecting "endswitch" or "case" or "default" (line 1) -- "switch (1):"
syntax error, unexpected end of file, expecting "endswitch" or "case" or "default" (line 2) -- "switch (1): case 1:\n"
syntax error, unexpected T_INLINE_HTML "  ", expecting "endswitch" or "case" or "default" (line 1) -- "switch (1): ?>  <?php case 1: endswitch;"
syntax error, unexpected T_INLINE_HTML "hello", expecting "endswitch" or "case" or "default" (line 2) -- "switch (1): ?>\nhello<?php case 1: endswitch;"
syntax error, unexpected token ";", expecting "endswitch" or "case" or "default" (line 1) -- "switch (1): ; ?><?php case 1: endswitch;"
Unclosed '{' on line 1 (line 2) -- "switch (1) { case 1: ?>A\n"
Unclosed '{' (line 1) -- "if (1) {"
Unclosed '{' on line 1 (line 3) -- "while (1) {\n\n"
two
two
two
[other]
after a comment-only block
empty
empty alt
