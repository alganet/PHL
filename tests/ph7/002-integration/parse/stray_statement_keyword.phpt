--TEST--
A keyword that opens no statement is php's unexpected token, with the tail its block names
--DESCRIPTION--
`else`, `endwhile`, `case`, `public` and the other words that cannot open a statement were
refused with our own sentence, `Syntax error: Unexpected keyword 'endwhile'`. php's parser
names them as tokens, and adds what the enclosing statement list was waiting for: the end of
the file outside every block (a braced namespace's body is a block), the remaining branches
of an alternative-syntax if/elseif body, a switch's case list -- and nothing anywhere else.
`var` outside a class took the same tail. After a braced namespace, such a word is that parse
error, not the "No code may exist outside of namespace {}" compile fatal.
--FILE--
<?php
$cases = [
    'endwhile;',
    'enddeclare;',
    '$a = 1; else;',
    'if (1); else; else;',
    'case 1: echo 1;',
    'default;',
    'catch;',
    'try {} finally {} finally;',
    'as;',
    'extends;',
    'implements;',
    'insteadof;',
    'public;',
    'protected function f() {}',
    'private $x;',
    'endforeach;',
    'var $x;',
    'namespace A;' . "\n" . 'endfor;',
    'namespace A { }' . "\n" . 'endswitch;',
    'namespace A { endwhile; }',
    'if (1) { endwhile; }',
    '{ catch; }',
    'function f() { elseif; }',
    'function g() { var $x; }',
    'class C { function m() { endif; } }',
    '$f = function () { endwhile; };',
    'declare(ticks=1) { endwhile; }',
    'if (1): endwhile; endif;',
    'if (1): elseif (2): endfor; endif;',
    'if (1): else: endwhile; endif;',
    'while (1): endif; endwhile;',
    'for (;;): endwhile; endfor;',
    'switch (1) { case 1: endwhile; }',
    'switch (1): case 1: endwhile; endswitch;',
];
foreach ($cases as $code) {
    try {
        eval($code);
        echo "no error: $code\n";
    } catch (ParseError $e) {
        echo str_replace("\n", '\n', $code), "\n  ", $e->getMessage(), "\n";
    }
}
--EXPECT--
endwhile;
  syntax error, unexpected token "endwhile", expecting end of file
enddeclare;
  syntax error, unexpected token "enddeclare", expecting end of file
$a = 1; else;
  syntax error, unexpected token "else", expecting end of file
if (1); else; else;
  syntax error, unexpected token "else", expecting end of file
case 1: echo 1;
  syntax error, unexpected token "case", expecting end of file
default;
  syntax error, unexpected token "default", expecting end of file
catch;
  syntax error, unexpected token "catch", expecting end of file
try {} finally {} finally;
  syntax error, unexpected token "finally", expecting end of file
as;
  syntax error, unexpected token "as", expecting end of file
extends;
  syntax error, unexpected token "extends", expecting end of file
implements;
  syntax error, unexpected token "implements", expecting end of file
insteadof;
  syntax error, unexpected token "insteadof", expecting end of file
public;
  syntax error, unexpected token "public", expecting end of file
protected function f() {}
  syntax error, unexpected token "protected", expecting end of file
private $x;
  syntax error, unexpected token "private", expecting end of file
endforeach;
  syntax error, unexpected token "endforeach", expecting end of file
var $x;
  syntax error, unexpected token "var", expecting end of file
namespace A;\nendfor;
  syntax error, unexpected token "endfor", expecting end of file
namespace A { }\nendswitch;
  syntax error, unexpected token "endswitch", expecting end of file
namespace A { endwhile; }
  syntax error, unexpected token "endwhile"
if (1) { endwhile; }
  syntax error, unexpected token "endwhile"
{ catch; }
  syntax error, unexpected token "catch"
function f() { elseif; }
  syntax error, unexpected token "elseif"
function g() { var $x; }
  syntax error, unexpected token "var"
class C { function m() { endif; } }
  syntax error, unexpected token "endif"
$f = function () { endwhile; };
  syntax error, unexpected token "endwhile"
declare(ticks=1) { endwhile; }
  syntax error, unexpected token "endwhile"
if (1): endwhile; endif;
  syntax error, unexpected token "endwhile", expecting "elseif" or "else" or "endif"
if (1): elseif (2): endfor; endif;
  syntax error, unexpected token "endfor", expecting "elseif" or "else" or "endif"
if (1): else: endwhile; endif;
  syntax error, unexpected token "endwhile"
while (1): endif; endwhile;
  syntax error, unexpected token "endif"
for (;;): endwhile; endfor;
  syntax error, unexpected token "endwhile"
switch (1) { case 1: endwhile; }
  syntax error, unexpected token "endwhile", expecting "case" or "default" or "}"
switch (1): case 1: endwhile; endswitch;
  syntax error, unexpected token "endwhile", expecting "endswitch" or "case" or "default"
