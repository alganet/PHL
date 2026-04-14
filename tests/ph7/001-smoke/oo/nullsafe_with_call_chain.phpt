--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe short-circuits across chained method calls
--FILE--
<?php
class NsfCallChainChain {
    public function getSelf() { echo "SHOULD_NOT_RUN\n"; return $this; }
    public function getName() { echo "SHOULD_NOT_RUN\n"; return "x"; }
}
$nsfCallChain_c = null;
$nsfCallChain_r = $nsfCallChain_c?->getSelf()->getName();
echo ($nsfCallChain_r === null ? "yes" : "no"), "\n";
?>
--EXPECT--
yes
--CLEAN--
<?php
unset($nsfCallChain_c, $nsfCallChain_r);
