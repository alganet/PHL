--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A user `final readonly` base is refused for being final too
--DESCRIPTION--
The other half of number_final_readonly_extends: the rule is not BcMath\Number's,
it is `final readonly`'s, so a class the SCRIPT declares that way answers the same
way. Its own file because a compile-time fatal ends the process (ECOSYSTEM.md F30),
so one refusal is all a process can show.
--FILE--
<?php
final readonly class BcNumFinalRo {}
class BcNumSubB extends BcNumFinalRo {}
echo "unreached\n";
?>
--EXPECTF--
%AFatal error:%AClass BcNumSubB cannot extend final class BcNumFinalRo%A
