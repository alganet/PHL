--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `;` where a for loop's third clause should be is php's parse error, not an empty clause
--FILE--
<?php
// A for head has exactly two `;`. The third clause is an expression list
// closed by `)`, so a further `;` -- with or without an expression around
// it -- is the token php names, `expecting ")"`. Every body breaks at once,
// so an engine that accepts the head still stops.
$cases = [
    'a third `;`, nothing else'      => 'for (;;;) { break; }',
    'a third `;` before an expr'     => 'for (;;;1) { break; }',
    'two more `;`'                   => 'for (;; ;;) { break; }',
    'full clauses, then a `;`'       => 'for ($i = 0; $i < 1;;) { break; }',
    'an expr, then a `;`'            => 'for (;;$i++;) { break; }',
    'the `;` a line later'           => "for (;;\n;) { break; }",
    'alternative syntax'             => 'for (;;;): break; endfor;',
    'a single-statement body'        => 'for (;;;) break;',
];
foreach ($cases as $name => $src) {
    try {
        eval($src);
        echo "$name: compiled\n";
    } catch (ParseError $e) {
        echo "$name: ", $e->getMessage(), ' @', $e->getLine(), "\n";
    }
}
// The two-`;` head is untouched.
for ($i = 0; $i < 3; $i++) echo $i;
for (;;) { echo "|"; break; }
echo "\n";
--EXPECT--
a third `;`, nothing else: syntax error, unexpected token ";", expecting ")" @1
a third `;` before an expr: syntax error, unexpected token ";", expecting ")" @1
two more `;`: syntax error, unexpected token ";", expecting ")" @1
full clauses, then a `;`: syntax error, unexpected token ";", expecting ")" @1
an expr, then a `;`: syntax error, unexpected token ";", expecting ")" @1
the `;` a line later: syntax error, unexpected token ";", expecting ")" @2
alternative syntax: syntax error, unexpected token ";", expecting ")" @1
a single-statement body: syntax error, unexpected token ";", expecting ")" @1
012|
