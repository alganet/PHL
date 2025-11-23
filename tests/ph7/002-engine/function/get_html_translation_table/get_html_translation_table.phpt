--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_html_translation_table returns mapping array
--FILE--
<?php
$table = get_html_translation_table();
$pr = print_r($table, true);
echo (strpos($pr, '&lt;') !== false) ? 'get_html_table_ok' : 'get_html_table_fail';
?>
--EXPECT--
get_html_table_ok
--CLEAN--
<?php
unset($table, $pr);
?>
