--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode named entities: case-sensitive, semicolon required, unknown verbatim
--FILE--
<?php
echo html_entity_decode("&eacute;&Eacute;"), "\n";
echo html_entity_decode("&LT;&lt;"), "\n";
echo html_entity_decode("&eacute &eacute"), "\n";
echo html_entity_decode("&nosuch;&"), "\n";
echo html_entity_decode("&amp;lt;"), "\n";
?>
--EXPECT--
éÉ
&LT;<
&eacute &eacute
&nosuch;&
&lt;
--CLEAN--
<?php
