--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unset() cannot target a nullsafe chain
--FILE--
<?php
class NsfUnsetBox { public $x = 1; }
$nsfUnset_a = new NsfUnsetBox();
unset($nsfUnset_a?->x);
echo "never\n";
?>
--EXPECTF--
%ACan't use nullsafe operator in write context%A
--CLEAN--
<?php
