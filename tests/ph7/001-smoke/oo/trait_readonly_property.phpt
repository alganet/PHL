--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait may declare a private readonly typed property
--FILE--
<?php
class TrpState { public int $v = 41; }
trait TrpApi {
    private readonly TrpState $__state;
    public function state(): TrpState { return $this->__state ?? new TrpState(); }
}
class TrpUser { use TrpApi; }
$u = new TrpUser();
echo $u->state()->v + 1, "\n";
?>
--EXPECT--
42
