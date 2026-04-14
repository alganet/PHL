--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Compound assignment (+=) targeting a nullsafe chain is a parse error
--FILE--
<?php
class NsfCompoundBox { public $v = 1; }
$nsfCompound_a = new NsfCompoundBox();
$nsfCompound_a?->v += 1;
echo "never\n";
?>
--EXPECTF--
%ACan't use nullsafe operator in write context%A
--CLEAN--
<?php
