--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A FINAL readonly base is refused for being final, not for the readonly mismatch
--DESCRIPTION--
BcMath\Number is the engine's first `final readonly` class, and both refusals
applied to it: php reports only the FINAL one, and PHL reported the readonly
mismatch instead -- the right answer for the wrong reason, and the wrong reason
for a class nothing may extend either way.

It used to run four cases through eval() and CATCH each refusal, which php does
not allow: a compile-time fatal is uncatchable there and ends the process. Now
that this engine agrees, one refusal is all a process can show; the readonly
mismatch rows moved to oo/readonly_class_extend_nonreadonly.phpt and
oo/readonly_class_readonly_extend_plain.phpt, which already pinned them.
--FILE--
<?php
class BcNumSubA extends BcMath\Number {}
echo "unreached\n";
?>
--EXPECTF--
%AFatal error:%AClass BcNumSubA cannot extend final class BcMath\Number%A
