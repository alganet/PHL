--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named function has no class scope for `self`, wherever it is written
--DESCRIPTION--
A CLOSURE written inside a method keeps the enclosing class's scope for these
keywords -- php lets it, because a closure's scope is decided when it is bound.
A named FUNCTION written in the same place does not: php refuses it exactly as
it refuses one at top level. A compile fatal runs nothing, so what the accepted
closure proves is that it COMPILED.
--FILE--
<?php
class TskB {
    public function m() {
        $ok = function (self $a) {};
        return $ok;
    }
}
class TskC {
    public function m() {
        function tskNested(self $a) {}
    }
}
echo "unreachable\n";
?>
--EXPECTF--
%ACannot use "self" when no class scope is active%A
--CLEAN--
<?php
