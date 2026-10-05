--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A use or namespace statement anywhere but the top level is a parse error
--FILE--
<?php
// php's grammar takes `use` and `namespace` as top statements only: at file
// scope or directly inside a namespace's braces. Anywhere else each is a parse
// error on the keyword, and an alternative-syntax if/elseif body or a switch's
// case list name what the parser wanted instead.
$cases = [
    'function utlo_f1() { use Foo\Bar; }',
    'function utlo_f2() { echo 1; use Foo\Bar; }',
    'function utlo_f3() { use function Foo\bar; }',
    'function utlo_f4() { use const Foo\BAR; }',
    'function utlo_f5() { use Foo\{Bar, Baz}; }',
    "function utlo_f6() {\n    use\n        Foo\Bar;\n}",
    '{ use Foo\Bar; }',
    'if (1) { use Foo\Bar; }',
    'if (1) use Foo\Bar;',
    'if (1): use Foo\Bar; endif;',
    'if (1): elseif (2): use Foo\Bar; endif;',
    'if (1): else: use Foo\Bar; endif;',
    'if (1): { use Foo\Bar; } endif;',
    'while (0) use Foo\Bar;',
    'switch (1) { case 1: use Foo\Bar; }',
    'switch (1): case 1: use Foo\Bar; endswitch;',
    'declare(ticks=1) { use Foo\Bar; }',
    'try { use Foo\Bar; } finally {}',
    'class UtloC1 { function m() { use Foo\Bar; } }',
    '$f = function () { use Foo\Bar; };',
    'namespace UtloN1 { if (1) { use Foo\Bar; } }',
    'namespace UtloN2 { function f() { use Foo\Bar; } }',
    '{ namespace UtloN3; }',
    '{ namespace UtloN4 { } }',
    'function utlo_f7() { namespace UtloN5; }',
    'if (1) namespace UtloN6;',
    'if (1): namespace UtloN7; endif;',
    'switch (1) { case 1: namespace UtloN8; }',
    'switch (1): case 1: namespace UtloN9; endswitch;',
    '$f = function () { namespace UtloN10 { echo 1; } };',
];
foreach ($cases as $code) {
    try {
        eval($code);
        echo "compiled: $code\n";
    } catch (ParseError $e) {
        echo $e->getMessage(), ' (line ', $e->getLine(), ') -- ', $code, "\n";
    }
}

// The top-level places still import and declare, and a nested namespace
// declaration still reaches its own compile-time refusal.
eval('use Foo\Bar; echo Bar::class, "\n";');
eval('namespace UtloN11 { use Foo\Bar as B; echo B::class, "\n"; }');
eval('namespace UtloN12; use function Foo\baz; echo __NAMESPACE__, "\n";');
function utlo_g() {
    eval('use Foo\Qux; echo Qux::class, "\n";');
    eval('namespace UtloN13; echo __NAMESPACE__, "\n";');
    namespace\utlo_h();
}
function utlo_h() { echo "relative name\n"; }
utlo_g();
--EXPECT--
syntax error, unexpected token "use" (line 1) -- function utlo_f1() { use Foo\Bar; }
syntax error, unexpected token "use" (line 1) -- function utlo_f2() { echo 1; use Foo\Bar; }
syntax error, unexpected token "use" (line 1) -- function utlo_f3() { use function Foo\bar; }
syntax error, unexpected token "use" (line 1) -- function utlo_f4() { use const Foo\BAR; }
syntax error, unexpected token "use" (line 1) -- function utlo_f5() { use Foo\{Bar, Baz}; }
syntax error, unexpected token "use" (line 2) -- function utlo_f6() {
    use
        Foo\Bar;
}
syntax error, unexpected token "use" (line 1) -- { use Foo\Bar; }
syntax error, unexpected token "use" (line 1) -- if (1) { use Foo\Bar; }
syntax error, unexpected token "use" (line 1) -- if (1) use Foo\Bar;
syntax error, unexpected token "use", expecting "elseif" or "else" or "endif" (line 1) -- if (1): use Foo\Bar; endif;
syntax error, unexpected token "use", expecting "elseif" or "else" or "endif" (line 1) -- if (1): elseif (2): use Foo\Bar; endif;
syntax error, unexpected token "use" (line 1) -- if (1): else: use Foo\Bar; endif;
syntax error, unexpected token "use" (line 1) -- if (1): { use Foo\Bar; } endif;
syntax error, unexpected token "use" (line 1) -- while (0) use Foo\Bar;
syntax error, unexpected token "use", expecting "case" or "default" or "}" (line 1) -- switch (1) { case 1: use Foo\Bar; }
syntax error, unexpected token "use", expecting "endswitch" or "case" or "default" (line 1) -- switch (1): case 1: use Foo\Bar; endswitch;
syntax error, unexpected token "use" (line 1) -- declare(ticks=1) { use Foo\Bar; }
syntax error, unexpected token "use" (line 1) -- try { use Foo\Bar; } finally {}
syntax error, unexpected token "use" (line 1) -- class UtloC1 { function m() { use Foo\Bar; } }
syntax error, unexpected token "use" (line 1) -- $f = function () { use Foo\Bar; };
syntax error, unexpected token "use" (line 1) -- namespace UtloN1 { if (1) { use Foo\Bar; } }
syntax error, unexpected token "use" (line 1) -- namespace UtloN2 { function f() { use Foo\Bar; } }
syntax error, unexpected token "namespace" (line 1) -- { namespace UtloN3; }
syntax error, unexpected token "namespace" (line 1) -- { namespace UtloN4 { } }
syntax error, unexpected token "namespace" (line 1) -- function utlo_f7() { namespace UtloN5; }
syntax error, unexpected token "namespace" (line 1) -- if (1) namespace UtloN6;
syntax error, unexpected token "namespace", expecting "elseif" or "else" or "endif" (line 1) -- if (1): namespace UtloN7; endif;
syntax error, unexpected token "namespace", expecting "case" or "default" or "}" (line 1) -- switch (1) { case 1: namespace UtloN8; }
syntax error, unexpected token "namespace", expecting "endswitch" or "case" or "default" (line 1) -- switch (1): case 1: namespace UtloN9; endswitch;
syntax error, unexpected token "namespace" (line 1) -- $f = function () { namespace UtloN10 { echo 1; } };
Foo\Bar
Foo\Bar
UtloN12
Foo\Qux
UtloN13
relative name
