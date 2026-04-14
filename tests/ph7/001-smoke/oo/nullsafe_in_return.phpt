--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe expression used as a return value
--FILE--
<?php
class NsfReturnItem { public $label = "widget"; }
function nsfReturn_labelOf($item) {
    return $item?->label;
}
echo (nsfReturn_labelOf(null) === null ? "null" : "no"), "\n";
echo nsfReturn_labelOf(new NsfReturnItem()), "\n";
?>
--EXPECT--
null
widget
--CLEAN--
<?php
