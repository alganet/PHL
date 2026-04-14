--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe on the second link short-circuits when the intermediate value is null
--FILE--
<?php
class NsfSecondLinkHolder { public $child = null; }
$nsfSecondLink_h = new NsfSecondLinkHolder();
$nsfSecondLink_r = $nsfSecondLink_h->child?->anything;
echo ($nsfSecondLink_r === null ? "yes" : "no"), "\n";
?>
--EXPECT--
yes
--CLEAN--
<?php
unset($nsfSecondLink_h, $nsfSecondLink_r);
