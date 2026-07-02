--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_html_translation_table quote rows follow the quote flags
--FILE--
<?php
echo count(get_html_translation_table(HTML_SPECIALCHARS, ENT_QUOTES)), "\n";
echo count(get_html_translation_table(HTML_SPECIALCHARS, ENT_COMPAT)), "\n";
echo count(get_html_translation_table(HTML_SPECIALCHARS, ENT_NOQUOTES)), "\n";
$t = get_html_translation_table(HTML_SPECIALCHARS, ENT_COMPAT);
echo isset($t["'"]) ? "sq" : "no-sq", " ", isset($t['"']) ? "dq" : "no-dq", "\n";
?>
--EXPECT--
5
4
3
no-sq dq
--CLEAN--
<?php
unset($t);
