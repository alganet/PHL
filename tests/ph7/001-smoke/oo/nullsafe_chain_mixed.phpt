--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe chain passes through when LHS is a real object
--FILE--
<?php
class NsfChainMixedInner { public $name = "inner"; }
class NsfChainMixedOuter { public $in; function __construct() { $this->in = new NsfChainMixedInner(); } }
$nsfChainMixed_o = new NsfChainMixedOuter();
echo $nsfChainMixed_o?->in->name, "\n";
?>
--EXPECT--
inner
--CLEAN--
<?php
unset($nsfChainMixed_o);
