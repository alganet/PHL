--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe method call on null returns null and never runs the body
--FILE--
<?php
class NsfMethodNullGreeter {
    public function greet() { echo "SHOULD_NOT_RUN\n"; return "hi"; }
}
$nsfMethodNull_g = null;
$nsfMethodNull_r = $nsfMethodNull_g?->greet();
echo ($nsfMethodNull_r === null ? "yes" : "no"), "\n";
?>
--EXPECT--
yes
--CLEAN--
<?php
unset($nsfMethodNull_g, $nsfMethodNull_r);
