--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A modifier written twice is php's per-modifier sentence
--DESCRIPTION--
The member-modifier run reads each modifier at most once; php words the second
occurrence per modifier (`final`, `static`, `abstract`, `readonly`) and gives the
two VISIBILITY kinds one shared sentence, which is also what it says for two
DIFFERENT visibilities (`public private $p`).
--FILE--
<?php
class FmdC { final final public $p = 1; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Multiple final modifiers are not allowed %s
--CLEAN--
<?php
