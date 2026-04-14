--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe chain combined with null coalescing ?? falls back on null
--FILE--
<?php
class NsfCoalesceCfg { public $name = "alice"; }
$nsfCoalesce_a = null;
echo ($nsfCoalesce_a?->name ?? "default"), "\n";
$nsfCoalesce_b = new NsfCoalesceCfg();
echo ($nsfCoalesce_b?->name ?? "default"), "\n";
?>
--EXPECT--
default
alice
--CLEAN--
<?php
unset($nsfCoalesce_a, $nsfCoalesce_b);
