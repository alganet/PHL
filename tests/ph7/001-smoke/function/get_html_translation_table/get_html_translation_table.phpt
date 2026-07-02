--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_html_translation_table default table: char => entity specials
--FILE--
<?php
$t = get_html_translation_table();
echo count($t), "\n";
echo $t['<'], " ", $t['>'], " ", $t['&'], " ", $t['"'], " ", $t["'"], "\n";
?>
--EXPECT--
5
&lt; &gt; &amp; &quot; &#039;
--CLEAN--
<?php
unset($t);
