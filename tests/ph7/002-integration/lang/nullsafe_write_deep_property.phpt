--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe anywhere in an assignment LHS chain is a parse error, even when the outer op is `->`
--FILE--
<?php
class NsfWriteDeepBox { public $b; }
$nsfWriteDeep_a = new NsfWriteDeepBox();
$nsfWriteDeep_a->b = new NsfWriteDeepBox();
$nsfWriteDeep_a?->b->c = 1;
echo "never\n";
?>
--EXPECTF--
%ACan't use nullsafe operator in write context%A
--CLEAN--
<?php
