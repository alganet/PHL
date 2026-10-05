--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A statement head with no `(`, or with one nothing closes, is refused by whichever of php's scanner and parser meets it first
--FILE--
<?php
// php has no sentence of its own for a malformed `if (`, `while (`, `for (`,
// `foreach (` or `switch (`. Without the `(` its parser names the token it
// found instead. With a `(` nothing closes, the parser names the `;` or the
// `{` that cannot continue the head -- and what it was still waiting for,
// where that is few enough tokens to list -- while the scanner names the
// BRACKET: the innermost one still open when the input ends, or when a closer
// of another kind arrives.
$heads = [
    'if'      => ['if', '1', ' { echo 1; }'],
    'elseif'  => ["if (0) { }\nelseif", '1', ' { echo 1; }'],
    'else if' => ["if (0) { }\nelse if", '1', ' { echo 1; }'],
    'while'   => ['while', '0', ' { echo 1; }'],
    'do'      => ["do { }\nwhile", '0', ';'],
    'switch'  => ['switch', '1', ' { case 1: }'],
    'for'     => ['for', ';0;', ' { echo 1; }'],
    'foreach' => ['foreach', '[] as $v', ' { echo 1; }'],
];
$cases = [];
$n = 0;
foreach ($heads as $name => [$kw, $in, $body]) {
    $n++;
    $cases["$name, `;` for the `(`"]        = "$kw;";
    $cases["$name, the input ends"]         = "$kw";
    $cases["$name, it ends a line later"]   = "$kw\n";
    $cases["$name, no `(`"]                 = "$kw $in)$body";
    $cases["$name, a `{` for the `(`"]      = "$kw { }";
    $cases["$name, `()`"]                   = "$kw ()$body";
    $cases["$name, no `)`"]                 = "$kw ($in$body";
    $cases["$name, no `)` then `;`"]        = "$kw ($in;\necho 2;";
    $cases["$name, `(` then the end"]       = "$kw (";
    $cases["$name, `(` then a line"]        = "$kw (\n";
    $cases["$name, no `)` then the end"]    = "$kw ($in";
    $cases["$name, no `)` then two lines"]  = "$kw ($in\n\n";
    $cases["$name, an inner `(` unclosed"]  = "$kw (($in)$body";
    $cases["$name, `]` for the `)`"]        = "$kw ($in]$body";
    $cases["$name, inside a function"]      = "function ush_f$n() {\n$kw ($in\n}\n";
    $cases["$name, the function unclosed"]  = "function ush_g$n() {\n$kw ($in\n";
}
$cases += [
    'for, nothing before the `{`'      => 'for ( { }',
    'for, in its first clause'         => 'for (1 { }',
    'for, in its second clause'        => 'for (1;2 { }',
    'for, in its third clause'         => 'for (1;2;3 { }',
    'for, a fourth `;`'                => 'for (1;2;3; echo 1;',
    'for, a comma list'                => 'for (1,2 { }',
    'foreach, nothing before the `{`'  => 'foreach ( { }',
    'foreach, before the `as`'         => 'foreach ([] { }',
    'foreach, after the `as`'          => 'foreach ([] as { }',
    'foreach, after the `=>`'          => 'foreach ([] as $k => { }',
    'foreach, after both targets'      => 'foreach ([] as $k => $v { }',
    'foreach, after a list target'     => 'foreach ([] as [$a] { }',
    'foreach, `;` before the `as`'     => 'foreach ($a; echo 1;',
    'foreach, an `as` in a group'      => 'foreach (([] as $v) { }',
    'a closure, then the end'          => 'if (function() { return 1; }',
    'a closure, then the body'         => 'if (function() { return 1; } { echo 1; }',
    'a match, then the body'           => 'if (match(1) { 1 => 2 } { echo 1; }',
    'an anonymous class'               => 'if (new class { } { echo 1; }',
    'a braced property'                => 'if ($o->{"a"} { echo 1; }',
    'a braced variable'                => 'if (${"a"} { echo 1; }',
    '::class owes no body'             => 'if (X::class { echo 1; }',
    'an argument list'                 => 'if (ush_h(1 { }',
    'an argument list, `;`'            => 'while (ush_h(1; echo 1;',
    'a subscript'                      => 'if ($a[1 { }',
    'the innermost bracket'            => "if (ush_h([1\n",
    'the innermost, a wrong closer'    => "if (ush_h([1\n)",
    'a wrong inner closer'             => 'if ((1] ) { echo 1; }',
    'a `}` for the `)`'                => 'if (1 }',
    'a `}`, inside a function'         => 'function ush_i() { if (1 }',
    'an operator, then the body'       => 'while (1 + { }',
    'an operator, then the end'        => "if (1 +\n",
    'an operand too many'              => 'if (1 2 { }',
    'a comma'                          => 'if (1,2 { }',
    'a closing tag'                    => 'if (1 ?> x',
    'a closing tag, in a for'          => 'for (1;2;3 ?> x',
];
foreach ($cases as $label => $code) {
    try {
        eval($code);
        echo "$label: compiled\n";
    } catch (ParseError $e) {
        echo "$label: ", $e->getMessage(), " @", $e->getLine(), "\n";
    }
}
// Heads that ARE closed keep working with every brace an expression may hold.
if (function() { return 1; }) { echo "closure\n"; }
if (match(1) { 1 => true }) { echo "match\n"; }
while ((new class { public $p = 0; })->p) { }
foreach ((function() { foreach ([1] as $x) { yield $x; } })() as $v) { echo "gen $v\n"; }
for ($i = (function() { return 0; })(); $i < 1; $i++) { echo "for\n"; }
switch ((function() { return 2; })()) { case 2: echo "switch\n"; }
--EXPECT--
if, `;` for the `(`: syntax error, unexpected token ";", expecting "(" @1
if, the input ends: syntax error, unexpected end of file, expecting "(" @1
if, it ends a line later: syntax error, unexpected end of file, expecting "(" @2
if, no `(`: syntax error, unexpected integer "1", expecting "(" @1
if, a `{` for the `(`: syntax error, unexpected token "{", expecting "(" @1
if, `()`: syntax error, unexpected token ")" @1
if, no `)`: syntax error, unexpected token "{" @1
if, no `)` then `;`: syntax error, unexpected token ";" @1
if, `(` then the end: Unclosed '(' @1
if, `(` then a line: Unclosed '(' on line 1 @2
if, no `)` then the end: Unclosed '(' @1
if, no `)` then two lines: Unclosed '(' on line 1 @3
if, an inner `(` unclosed: syntax error, unexpected token "{" @1
if, `]` for the `)`: Unclosed '(' does not match ']' @1
if, inside a function: Unclosed '(' on line 2 does not match '}' @3
if, the function unclosed: Unclosed '(' on line 2 @3
elseif, `;` for the `(`: syntax error, unexpected token ";", expecting "(" @2
elseif, the input ends: syntax error, unexpected end of file, expecting "(" @2
elseif, it ends a line later: syntax error, unexpected end of file, expecting "(" @3
elseif, no `(`: syntax error, unexpected integer "1", expecting "(" @2
elseif, a `{` for the `(`: syntax error, unexpected token "{", expecting "(" @2
elseif, `()`: syntax error, unexpected token ")" @2
elseif, no `)`: syntax error, unexpected token "{" @2
elseif, no `)` then `;`: syntax error, unexpected token ";" @2
elseif, `(` then the end: Unclosed '(' @2
elseif, `(` then a line: Unclosed '(' on line 2 @3
elseif, no `)` then the end: Unclosed '(' @2
elseif, no `)` then two lines: Unclosed '(' on line 2 @4
elseif, an inner `(` unclosed: syntax error, unexpected token "{" @2
elseif, `]` for the `)`: Unclosed '(' does not match ']' @2
elseif, inside a function: Unclosed '(' on line 3 does not match '}' @4
elseif, the function unclosed: Unclosed '(' on line 3 @4
else if, `;` for the `(`: syntax error, unexpected token ";", expecting "(" @2
else if, the input ends: syntax error, unexpected end of file, expecting "(" @2
else if, it ends a line later: syntax error, unexpected end of file, expecting "(" @3
else if, no `(`: syntax error, unexpected integer "1", expecting "(" @2
else if, a `{` for the `(`: syntax error, unexpected token "{", expecting "(" @2
else if, `()`: syntax error, unexpected token ")" @2
else if, no `)`: syntax error, unexpected token "{" @2
else if, no `)` then `;`: syntax error, unexpected token ";" @2
else if, `(` then the end: Unclosed '(' @2
else if, `(` then a line: Unclosed '(' on line 2 @3
else if, no `)` then the end: Unclosed '(' @2
else if, no `)` then two lines: Unclosed '(' on line 2 @4
else if, an inner `(` unclosed: syntax error, unexpected token "{" @2
else if, `]` for the `)`: Unclosed '(' does not match ']' @2
else if, inside a function: Unclosed '(' on line 3 does not match '}' @4
else if, the function unclosed: Unclosed '(' on line 3 @4
while, `;` for the `(`: syntax error, unexpected token ";", expecting "(" @1
while, the input ends: syntax error, unexpected end of file, expecting "(" @1
while, it ends a line later: syntax error, unexpected end of file, expecting "(" @2
while, no `(`: syntax error, unexpected integer "0", expecting "(" @1
while, a `{` for the `(`: syntax error, unexpected token "{", expecting "(" @1
while, `()`: syntax error, unexpected token ")" @1
while, no `)`: syntax error, unexpected token "{" @1
while, no `)` then `;`: syntax error, unexpected token ";" @1
while, `(` then the end: Unclosed '(' @1
while, `(` then a line: Unclosed '(' on line 1 @2
while, no `)` then the end: Unclosed '(' @1
while, no `)` then two lines: Unclosed '(' on line 1 @3
while, an inner `(` unclosed: syntax error, unexpected token "{" @1
while, `]` for the `)`: Unclosed '(' does not match ']' @1
while, inside a function: Unclosed '(' on line 2 does not match '}' @3
while, the function unclosed: Unclosed '(' on line 2 @3
do, `;` for the `(`: syntax error, unexpected token ";", expecting "(" @2
do, the input ends: syntax error, unexpected end of file, expecting "(" @2
do, it ends a line later: syntax error, unexpected end of file, expecting "(" @3
do, no `(`: syntax error, unexpected integer "0", expecting "(" @2
do, a `{` for the `(`: syntax error, unexpected token "{", expecting "(" @2
do, `()`: syntax error, unexpected token ")" @2
do, no `)`: syntax error, unexpected token ";" @2
do, no `)` then `;`: syntax error, unexpected token ";" @2
do, `(` then the end: Unclosed '(' @2
do, `(` then a line: Unclosed '(' on line 2 @3
do, no `)` then the end: Unclosed '(' @2
do, no `)` then two lines: Unclosed '(' on line 2 @4
do, an inner `(` unclosed: syntax error, unexpected token ";" @2
do, `]` for the `)`: Unclosed '(' does not match ']' @2
do, inside a function: Unclosed '(' on line 3 does not match '}' @4
do, the function unclosed: Unclosed '(' on line 3 @4
switch, `;` for the `(`: syntax error, unexpected token ";", expecting "(" @1
switch, the input ends: syntax error, unexpected end of file, expecting "(" @1
switch, it ends a line later: syntax error, unexpected end of file, expecting "(" @2
switch, no `(`: syntax error, unexpected integer "1", expecting "(" @1
switch, a `{` for the `(`: syntax error, unexpected token "{", expecting "(" @1
switch, `()`: syntax error, unexpected token ")" @1
switch, no `)`: syntax error, unexpected token "{" @1
switch, no `)` then `;`: syntax error, unexpected token ";" @1
switch, `(` then the end: Unclosed '(' @1
switch, `(` then a line: Unclosed '(' on line 1 @2
switch, no `)` then the end: Unclosed '(' @1
switch, no `)` then two lines: Unclosed '(' on line 1 @3
switch, an inner `(` unclosed: syntax error, unexpected token "{" @1
switch, `]` for the `)`: Unclosed '(' does not match ']' @1
switch, inside a function: Unclosed '(' on line 2 does not match '}' @3
switch, the function unclosed: Unclosed '(' on line 2 @3
for, `;` for the `(`: syntax error, unexpected token ";", expecting "(" @1
for, the input ends: syntax error, unexpected end of file, expecting "(" @1
for, it ends a line later: syntax error, unexpected end of file, expecting "(" @2
for, no `(`: syntax error, unexpected token ";", expecting "(" @1
for, a `{` for the `(`: syntax error, unexpected token "{", expecting "(" @1
for, `()`: syntax error, unexpected token ")", expecting ";" @1
for, no `)`: syntax error, unexpected token "{", expecting ")" @1
for, no `)` then `;`: syntax error, unexpected token ";", expecting ")" @1
for, `(` then the end: Unclosed '(' @1
for, `(` then a line: Unclosed '(' on line 1 @2
for, no `)` then the end: Unclosed '(' @1
for, no `)` then two lines: Unclosed '(' on line 1 @3
for, an inner `(` unclosed: syntax error, unexpected token ";" @1
for, `]` for the `)`: Unclosed '(' does not match ']' @1
for, inside a function: Unclosed '(' on line 2 does not match '}' @3
for, the function unclosed: Unclosed '(' on line 2 @3
foreach, `;` for the `(`: syntax error, unexpected token ";", expecting "(" @1
foreach, the input ends: syntax error, unexpected end of file, expecting "(" @1
foreach, it ends a line later: syntax error, unexpected end of file, expecting "(" @2
foreach, no `(`: syntax error, unexpected token "[", expecting "(" @1
foreach, a `{` for the `(`: syntax error, unexpected token "{", expecting "(" @1
foreach, `()`: syntax error, unexpected token ")" @1
foreach, no `)`: syntax error, unexpected token "{", expecting "->" or "?->" or "[" @1
foreach, no `)` then `;`: syntax error, unexpected token ";", expecting "->" or "?->" or "[" @1
foreach, `(` then the end: Unclosed '(' @1
foreach, `(` then a line: Unclosed '(' on line 1 @2
foreach, no `)` then the end: Unclosed '(' @1
foreach, no `)` then two lines: Unclosed '(' on line 1 @3
foreach, an inner `(` unclosed: syntax error, unexpected token "as" @1
foreach, `]` for the `)`: Unclosed '(' does not match ']' @1
foreach, inside a function: Unclosed '(' on line 2 does not match '}' @3
foreach, the function unclosed: Unclosed '(' on line 2 @3
for, nothing before the `{`: syntax error, unexpected token "{", expecting ";" @1
for, in its first clause: syntax error, unexpected token "{", expecting ";" @1
for, in its second clause: syntax error, unexpected token "{", expecting ";" @1
for, in its third clause: syntax error, unexpected token "{", expecting ")" @1
for, a fourth `;`: syntax error, unexpected token ";", expecting ")" @1
for, a comma list: syntax error, unexpected token "{", expecting ";" @1
foreach, nothing before the `{`: syntax error, unexpected token "{" @1
foreach, before the `as`: syntax error, unexpected token "{" @1
foreach, after the `as`: syntax error, unexpected token "{" @1
foreach, after the `=>`: syntax error, unexpected token "{" @1
foreach, after both targets: syntax error, unexpected token "{", expecting "->" or "?->" or "[" @1
foreach, after a list target: syntax error, unexpected token "{", expecting "->" or "?->" or "[" @1
foreach, `;` before the `as`: syntax error, unexpected token ";" @1
foreach, an `as` in a group: syntax error, unexpected token "as" @1
a closure, then the end: Unclosed '(' @1
a closure, then the body: syntax error, unexpected token "{" @1
a match, then the body: syntax error, unexpected token "{" @1
an anonymous class: syntax error, unexpected token "{" @1
a braced property: syntax error, unexpected token "{" @1
a braced variable: syntax error, unexpected token "{" @1
::class owes no body: syntax error, unexpected token "{" @1
an argument list: syntax error, unexpected token "{", expecting ")" @1
an argument list, `;`: syntax error, unexpected token ";", expecting ")" @1
a subscript: syntax error, unexpected token "{", expecting "]" @1
the innermost bracket: Unclosed '[' on line 1 @2
the innermost, a wrong closer: Unclosed '[' on line 1 does not match ')' @2
a wrong inner closer: Unclosed '(' does not match ']' @1
a `}` for the `)`: Unclosed '(' does not match '}' @1
a `}`, inside a function: Unclosed '(' does not match '}' @1
an operator, then the body: syntax error, unexpected token "{" @1
an operator, then the end: Unclosed '(' on line 1 @2
an operand too many: syntax error, unexpected integer "2" @1
a comma: syntax error, unexpected token "," @1
a closing tag: syntax error, unexpected token ";" @1
a closing tag, in a for: syntax error, unexpected token ";", expecting ")" @1
closure
match
gen 1
for
switch
