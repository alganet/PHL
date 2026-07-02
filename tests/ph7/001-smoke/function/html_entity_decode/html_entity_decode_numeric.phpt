--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode numeric refs: dec/hex, leading zeros, range and control gating
--FILE--
<?php
echo html_entity_decode("&#233;&#0233;&#xE9;&#XE9;&#x0000E9;"), "\n";
echo html_entity_decode("&#x110000;&#xD800;"), "\n";            // out of range / surrogate: verbatim
echo bin2hex(html_entity_decode("&#0;&#31;")), "\n";            // HTML401 disallows C0
echo bin2hex(html_entity_decode("&#9;&#10;&#13;")), "\n";       // ... except TAB/LF/CR
echo bin2hex(html_entity_decode("&#x7F;&#x9F;")), "\n";         // DEL..U+009F verbatim under HTML401
echo bin2hex(html_entity_decode("&#x7F;", ENT_QUOTES | ENT_XML1)), "\n"; // but XML1 allows it
?>
--EXPECT--
ééééé
&#x110000;&#xD800;
2623303b262333313b
090a0d
26237837463b26237839463b
7f
--CLEAN--
<?php
