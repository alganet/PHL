--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A const statement anywhere but the top level is a parse error
--FILE--
<?php
// php's grammar takes `const` as a top statement only: at file scope or
// directly inside a namespace's braces. Anywhere else it is a parse error on
// the keyword, and an alternative-syntax if/elseif body or a switch's case
// list name what the parser wanted instead.
$cases = [
    'function f1() { const X = 1; }',
    'function f2() { echo 1; const X = 1; }',
    '{ const X = 1; }',
    'if (1) { const X = 1; }',
    'if (1) const X = 1;',
    'if (1) {} else const X = 1;',
    'if (1): const X = 1; endif;',
    'if (1): elseif (2): const X = 1; endif;',
    'if (1): else: const X = 1; endif;',
    'if (1): if (2) { const X = 1; } endif;',
    'while (0) const X = 1;',
    'while (0): const X = 1; endwhile;',
    'do const X = 1; while (0);',
    'for (;0;): const X = 1; endfor;',
    'foreach ([] as $v): const X = 1; endforeach;',
    'switch (1) { case 1: const X = 1; }',
    'switch (1) { default: echo 1; const X = 1; }',
    'switch (1) { case 1: { const X = 1; } }',
    'switch (1): case 1: const X = 1; endswitch;',
    'declare(ticks=1) { const X = 1; }',
    'try { const X = 1; } finally {}',
    'try {} catch (Exception $e) { const X = 1; }',
    'class C3 { function m() { const X = 1; } }',
    '$f = function () { const X = 1; };',
    '$f = fn() => function () { const X = 1; };',
    'namespace N1 { if (1) { const X = 1; } }',
    'namespace N2 { function f() { const X = 1; } }',
    'if (1) { #[\Deprecated] const X = 1; }',
];
foreach ($cases as $code) {
    try {
        eval($code);
        echo "compiled: $code\n";
    } catch (ParseError $e) {
        echo $e->getMessage(), ' -- ', $code, "\n";
    }
}

// The top-level places still declare.
eval('const CTLO_T1 = 1;');
eval('namespace CtloN3 { const CTLO_T2 = 2; }');
eval('namespace CtloN4; const CTLO_T3 = 3;');
function ctlo_g() { eval('const CTLO_T4 = 4;'); }
ctlo_g();
var_dump(CTLO_T1, \CtloN3\CTLO_T2, \CtloN4\CTLO_T3, CTLO_T4);
--EXPECT--
syntax error, unexpected token "const" -- function f1() { const X = 1; }
syntax error, unexpected token "const" -- function f2() { echo 1; const X = 1; }
syntax error, unexpected token "const" -- { const X = 1; }
syntax error, unexpected token "const" -- if (1) { const X = 1; }
syntax error, unexpected token "const" -- if (1) const X = 1;
syntax error, unexpected token "const" -- if (1) {} else const X = 1;
syntax error, unexpected token "const", expecting "elseif" or "else" or "endif" -- if (1): const X = 1; endif;
syntax error, unexpected token "const", expecting "elseif" or "else" or "endif" -- if (1): elseif (2): const X = 1; endif;
syntax error, unexpected token "const" -- if (1): else: const X = 1; endif;
syntax error, unexpected token "const" -- if (1): if (2) { const X = 1; } endif;
syntax error, unexpected token "const" -- while (0) const X = 1;
syntax error, unexpected token "const" -- while (0): const X = 1; endwhile;
syntax error, unexpected token "const" -- do const X = 1; while (0);
syntax error, unexpected token "const" -- for (;0;): const X = 1; endfor;
syntax error, unexpected token "const" -- foreach ([] as $v): const X = 1; endforeach;
syntax error, unexpected token "const", expecting "case" or "default" or "}" -- switch (1) { case 1: const X = 1; }
syntax error, unexpected token "const", expecting "case" or "default" or "}" -- switch (1) { default: echo 1; const X = 1; }
syntax error, unexpected token "const" -- switch (1) { case 1: { const X = 1; } }
syntax error, unexpected token "const", expecting "endswitch" or "case" or "default" -- switch (1): case 1: const X = 1; endswitch;
syntax error, unexpected token "const" -- declare(ticks=1) { const X = 1; }
syntax error, unexpected token "const" -- try { const X = 1; } finally {}
syntax error, unexpected token "const" -- try {} catch (Exception $e) { const X = 1; }
syntax error, unexpected token "const" -- class C3 { function m() { const X = 1; } }
syntax error, unexpected token "const" -- $f = function () { const X = 1; };
syntax error, unexpected token "const" -- $f = fn() => function () { const X = 1; };
syntax error, unexpected token "const" -- namespace N1 { if (1) { const X = 1; } }
syntax error, unexpected token "const" -- namespace N2 { function f() { const X = 1; } }
syntax error, unexpected token "const" -- if (1) { #[\Deprecated] const X = 1; }
int(1)
int(2)
int(3)
int(4)
