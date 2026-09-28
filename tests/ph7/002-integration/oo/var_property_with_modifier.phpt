--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`var $x` takes no other modifier
--DESCRIPTION--
`var` is the pre-5.0 spelling of `public` and stands alone: php's parser is
looking for a VARIABLE where the modifier run left off, so `public var $x` is a
parse error naming the token that is not one. PHL used to ACCEPT `public var $p`
silently.
--FILE--
<?php
class VpmC { public var $p = 1; }
echo "unreached\n";
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token "var", expecting variable %s
--CLEAN--
<?php
