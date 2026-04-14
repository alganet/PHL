--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe operator cannot be used as the target of an assignment
--FILE--
<?php
class NsfWriteBox { public $x = 0; }
$nsfWrite_a = new NsfWriteBox();
$nsfWrite_a?->x = 5;
echo "never\n";
?>
--EXPECTF--
%ACan't use nullsafe operator in write context%A
--CLEAN--
<?php
