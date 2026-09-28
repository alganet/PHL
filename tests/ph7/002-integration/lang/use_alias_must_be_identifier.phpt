--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An import alias must be a plain label, not a reserved word
--DESCRIPTION--
php's `use` grammar takes a T_STRING after `as` and nothing else, and it names the offending
word LOWER-CASED however the source spelled it. The words PHL's own lexer hands back as
identifiers -- the alpha operators, `readonly`, `callable` -- are reserved to php all the
same, so they are refused here by name rather than by token class.
--FILE--
<?php
namespace UambiZ;
use UambiA\Q as Default;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token "default", expecting identifier%A
--CLEAN--
<?php
