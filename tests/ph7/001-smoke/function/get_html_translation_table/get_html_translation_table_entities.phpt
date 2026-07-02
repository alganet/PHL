--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_html_translation_table(HTML_ENTITIES): 253 rows, char => named entity
--FILE--
<?php
$t = get_html_translation_table(HTML_ENTITIES);
echo count($t), "\n";
echo $t["é"], " ", $t["€"], " ", $t["—"], " ", $t["\xC2\xA0"], "\n";
$keys = array_keys($t);
echo $keys[0] === '"' && $keys[1] === '&' ? "order-ok" : "order-bad", "\n";
?>
--EXPECT--
253
&eacute; &euro; &mdash; &nbsp;
order-ok
--CLEAN--
<?php
unset($t, $keys);
