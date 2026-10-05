--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An alternative-syntax body ends with its end keyword AND a `;`, and an input that ends inside one is a parse error
--FILE--
<?php
// php's grammar closes each body with `endif ';'` (and the four others the
// same way); a closing tag counts as the `;`. Anything else after the word is
// a parse error naming the token, and so is an input that ends before the
// word -- unless a `{` is still open around the body, which php's scanner
// refuses first.
$cases = [
    'if (1): echo ""; endif echo 2;',
    'if (0): else: endif 3;',
    'if (0): elseif (0): endif $x;',
    'while (0): endwhile echo 2;',
    'for (;0;): endfor echo 2;',
    'foreach ([] as $v): endforeach echo 2;',
    'switch (1): endswitch echo 2;',
    'switch (1): case 1: endswitch echo 2;',
    "while (0):\nendwhile\necho 2;",
    'if (1): while (0): endwhile endif;',
    'if (1): endif else: endif;',
    'function abt_f() { if (1): endif }',
    '{ while (0): endwhile }',
    'if (1): endif }',
    'while (0): endwhile )',
    'foreach ([] as $v): endforeach ]',
    'if (1): endif',
    "while (0): endwhile\n\n",
    'if (1): echo "";',
    'if (1):',
    'if (0): elseif (1):',
    'if (0): else:',
    'while (0):',
    'for (;0;): echo 1;',
    'foreach ([] as $v):',
    "while (0):\n\n",
    'if (1): ?>A<?php echo 1;',
    'if (1): if (0): endif;',
    '{ if (1):',
    'function abt_g() { while (0):',
];
foreach ($cases as $code) {
    try {
        eval($code);
        echo "compiled: ", json_encode($code), "\n";
    } catch (ParseError $e) {
        echo $e->getMessage(), ' (line ', $e->getLine(), ') -- ', json_encode($code), "\n";
    }
}

// The terminated forms, a closing tag among them.
eval('if (1): echo "a"; endif; echo "\n";');
eval('if (1): echo "b"; endif ?>' . "\n" . '<?php echo "\n";');
eval('while (0): endwhile; for (;0;): endfor; foreach ([] as $v): endforeach; echo "c\n";');
eval('switch (1): case 1: echo "d"; endswitch ?><?php echo "\n";');
eval('if (1): ?>[e]<?php endif ?>' . "\n");
--EXPECT--
syntax error, unexpected token "echo", expecting ";" (line 1) -- "if (1): echo \"\"; endif echo 2;"
syntax error, unexpected integer "3", expecting ";" (line 1) -- "if (0): else: endif 3;"
syntax error, unexpected variable "$x", expecting ";" (line 1) -- "if (0): elseif (0): endif $x;"
syntax error, unexpected token "echo", expecting ";" (line 1) -- "while (0): endwhile echo 2;"
syntax error, unexpected token "echo", expecting ";" (line 1) -- "for (;0;): endfor echo 2;"
syntax error, unexpected token "echo", expecting ";" (line 1) -- "foreach ([] as $v): endforeach echo 2;"
syntax error, unexpected token "echo", expecting ";" (line 1) -- "switch (1): endswitch echo 2;"
syntax error, unexpected token "echo", expecting ";" (line 1) -- "switch (1): case 1: endswitch echo 2;"
syntax error, unexpected token "echo", expecting ";" (line 3) -- "while (0):\nendwhile\necho 2;"
syntax error, unexpected token "endif", expecting ";" (line 1) -- "if (1): while (0): endwhile endif;"
syntax error, unexpected token "else", expecting ";" (line 1) -- "if (1): endif else: endif;"
syntax error, unexpected token "}", expecting ";" (line 1) -- "function abt_f() { if (1): endif }"
syntax error, unexpected token "}", expecting ";" (line 1) -- "{ while (0): endwhile }"
Unmatched '}' (line 1) -- "if (1): endif }"
Unmatched ')' (line 1) -- "while (0): endwhile )"
Unmatched ']' (line 1) -- "foreach ([] as $v): endforeach ]"
syntax error, unexpected end of file, expecting ";" (line 1) -- "if (1): endif"
syntax error, unexpected end of file, expecting ";" (line 3) -- "while (0): endwhile\n\n"
syntax error, unexpected end of file, expecting "elseif" or "else" or "endif" (line 1) -- "if (1): echo \"\";"
syntax error, unexpected end of file, expecting "elseif" or "else" or "endif" (line 1) -- "if (1):"
syntax error, unexpected end of file, expecting "elseif" or "else" or "endif" (line 1) -- "if (0): elseif (1):"
syntax error, unexpected end of file (line 1) -- "if (0): else:"
syntax error, unexpected end of file (line 1) -- "while (0):"
syntax error, unexpected end of file (line 1) -- "for (;0;): echo 1;"
syntax error, unexpected end of file (line 1) -- "foreach ([] as $v):"
syntax error, unexpected end of file (line 3) -- "while (0):\n\n"
syntax error, unexpected end of file, expecting "elseif" or "else" or "endif" (line 1) -- "if (1): ?>A<?php echo 1;"
syntax error, unexpected end of file, expecting "elseif" or "else" or "endif" (line 1) -- "if (1): if (0): endif;"
Unclosed '{' (line 1) -- "{ if (1):"
Unclosed '{' (line 1) -- "function abt_g() { while (0):"
a
b
c
d
[e]