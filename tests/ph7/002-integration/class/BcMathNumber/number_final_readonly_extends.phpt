--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A FINAL readonly base is refused for being final, not for the readonly mismatch
--DESCRIPTION--
BcMath\Number is the engine's first `final readonly` class, and both refusals
applied to it: php reports only the FINAL one, and PHL reported the readonly
mismatch instead -- the right answer for the wrong reason, and the wrong reason
for a class nothing may extend either way. Run here rather than in 001-smoke
because php's eval() fatal is uncatchable there and this one is a ParseError --
which is also why php skips it: the first refusal ends the process rather than
reaching the next three.
--SKIPIF--
<?php if (function_exists('zend_version')) echo "skip php's eval() fatal is uncatchable"; ?>
--FILE--
<?php
foreach (['class BcNumSubA extends BcMath\Number {}',
          'class BcNumSubB extends BcNumFinalRo {}',
          'class BcNumSubC extends BcNumPlainRo {}',
          'readonly class BcNumSubD extends BcNumPlain {}'] as $src) {
    try { eval($src); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
final readonly class BcNumFinalRo {}
readonly class BcNumPlainRo {}
class BcNumPlain {}
?>
--EXPECT--
ParseError: Class BcNumSubA cannot extend final class BcMath\Number
ParseError: Class BcNumSubB cannot extend final class BcNumFinalRo
ParseError: Non-readonly class BcNumSubC cannot extend readonly class BcNumPlainRo
ParseError: Readonly class BcNumSubD cannot extend non-readonly class BcNumPlain
