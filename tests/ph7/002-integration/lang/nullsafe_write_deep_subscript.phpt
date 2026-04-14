--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe anywhere in an assignment LHS chain is a parse error, even when the outer op is subscript
--FILE--
<?php
class NsfWriteDeepArr { public $arr = array(0); }
$nsfWriteDeep_a = new NsfWriteDeepArr();
$nsfWriteDeep_a?->arr[0] = 1;
echo "never\n";
?>
--EXPECTF--
%ACan't use nullsafe operator in write context%A
--CLEAN--
<?php
