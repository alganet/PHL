--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe chain short-circuits the entire postfix access chain when LHS is null
--FILE--
<?php
class NsfShortCircuitNever {
    public $z = 42;
    public function boom() { echo "SHOULD_NOT_RUN\n"; return $this; }
}
$nsfShortCircuit_a = null;
$nsfShortCircuit_r = $nsfShortCircuit_a?->b->c->boom()->z;
echo ($nsfShortCircuit_r === null ? "yes" : "no"), "\n";
?>
--EXPECT--
yes
--CLEAN--
<?php
unset($nsfShortCircuit_a, $nsfShortCircuit_r);
