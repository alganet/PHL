--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closer that does not match the innermost open bracket names that bracket, as php's scanner does
--FILE--
<?php
// php's scanner keeps one stack of the brackets open in a file and refuses a
// closer that is not the innermost one's before its parser sees it. It names
// that bracket -- `Unclosed '{' does not match ')'`, plus the line the bracket
// opened on when that is another line -- and says `Unmatched` only when
// nothing is open at all. The enclosing block's `{` counts, and so does one an
// earlier `<?php` block left open.
$cases = [
    'nothing open'                   => ') ;',
    'nothing open, a `]`'            => '] ;',
    'nothing open, a `}`'            => '}',
    'closed block, then a `)`'       => '{ $x = 1; } )',
    'inside a block'                 => '{ ) }',
    'inside a block, a line later'   => "{\n ) }",
    'inside a block, a `]`'          => 'if (1) { $a = 1; ] }',
    'past a call in a block'         => 'while (0) { $x = strlen("a")); }',
    'echo inside a block'            => '{ echo 1 ); }',
    'return inside a block'          => '{ return ); }',
    'a closed inner block'           => '{ { } ) }',
    'a function body'                => 'function f1() { ) }',
    'a function body, lines later'   => "function f2() {\n  \$a = 1;\n  ]; }",
    'a method body'                  => 'class C1 { function f() { $x = 1 ); } }',
    'a closure body'                 => '$f = function () { ) };',
    'a match arm'                    => '$x = match (1) { 1 => 2 ); };',
    'a `(` met by `]`'               => '$a = (1 ];',
    'a `(` met by `}`'               => '$a = (1 };',
    'an argument list met by `]`'    => 'strlen(1 ]);',
    'a subscript met by `)`'         => '{ $a = $b[1)]; }',
    'a short array met by `)`'       => '$a = [1 );',
    'a short array, a line later'    => "\$a = [\n1 );",
    'a short array in a head'        => 'if ([1) { }',
    'a nested short array'           => '$a = [1, [2) ];',
    'a `(` in a short array'         => '$a = [1, (2 ]];',
    'a call in a short array'        => '$a = [strlen(2 ]];',
    'a `{` left open by a block'     => '{ ?>x<?php ) ?>',
    'and on another line'            => "{\n?>x<?php ] ?>",
    'a `{` that block closed'        => '{ ?>x<?php } ?>y<?php ) ?>',
];
foreach ($cases as $name => $src) {
    try {
        eval($src);
        echo "$name: compiled\n";
    } catch (ParseError $e) {
        echo "$name: ", $e->getMessage(), ' @', $e->getLine(), "\n";
    }
}
// Brackets that match are untouched.
eval('{ $a = [1, (2), [3]]; echo json_encode($a), "\n"; }');
--EXPECT--
nothing open: Unmatched ')' @1
nothing open, a `]`: Unmatched ']' @1
nothing open, a `}`: Unmatched '}' @1
closed block, then a `)`: Unmatched ')' @1
inside a block: Unclosed '{' does not match ')' @1
inside a block, a line later: Unclosed '{' on line 1 does not match ')' @2
inside a block, a `]`: Unclosed '{' does not match ']' @1
past a call in a block: Unclosed '{' does not match ')' @1
echo inside a block: Unclosed '{' does not match ')' @1
return inside a block: Unclosed '{' does not match ')' @1
a closed inner block: Unclosed '{' does not match ')' @1
a function body: Unclosed '{' does not match ')' @1
a function body, lines later: Unclosed '{' on line 1 does not match ']' @3
a method body: Unclosed '{' does not match ')' @1
a closure body: Unclosed '{' does not match ')' @1
a match arm: Unclosed '{' does not match ')' @1
a `(` met by `]`: Unclosed '(' does not match ']' @1
a `(` met by `}`: Unclosed '(' does not match '}' @1
an argument list met by `]`: Unclosed '(' does not match ']' @1
a subscript met by `)`: Unclosed '[' does not match ')' @1
a short array met by `)`: Unclosed '[' does not match ')' @1
a short array, a line later: Unclosed '[' on line 1 does not match ')' @2
a short array in a head: Unclosed '[' does not match ')' @1
a nested short array: Unclosed '[' does not match ')' @1
a `(` in a short array: Unclosed '(' does not match ']' @1
a call in a short array: Unclosed '(' does not match ']' @1
a `{` left open by a block: Unclosed '{' does not match ')' @1
and on another line: Unclosed '{' on line 1 does not match ']' @2
a `{` that block closed: Unmatched ')' @1
[1,2,[3]]
