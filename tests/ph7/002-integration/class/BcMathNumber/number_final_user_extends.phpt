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
--SKIPIF--
<?php
// Both engines stop here and word it identically; php prints a `Stack trace:`
// block under a compile-time FATAL that this engine does not (ECOSYSTEM.md F30,
// behind F6's frame attribution).
if (function_exists('zend_version')) { echo 'skip php prints a Stack trace under a compile-time fatal'; }
?>
--FILE--
<?php
final readonly class BcNumFinalRo {}
class BcNumSubB extends BcNumFinalRo {}
echo "unreached\n";
?>
--EXPECTF--
%AFatal error:%AClass BcNumSubB cannot extend final class BcNumFinalRo%A
