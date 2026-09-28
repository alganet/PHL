--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`static` is no parameter type at all, however it is spelled
--DESCRIPTION--
php's grammar has no `static` in a parameter type: the bare and `&` spellings are
eaten by the modifier run ahead of the type (`Cannot use the static modifier on a
parameter`, which this engine already answered), and `?static` reaches the type
parser, where php's answer is the parse error its grammar produces. A RETURN type
`: static` is valid, and so is one on a property or a class constant -- this is a
parameter rule and a closure is not exempt from it. The `: static` return type
this leaves alone is in type_scope_keyword_accepts.phpt.
--FILE--
<?php
class TsnB { public function f(?static $a) {} }
echo "unreachable\n";
?>
--EXPECTF--
%Asyntax error, unexpected token "static"%A
--CLEAN--
<?php
