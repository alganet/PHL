--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml parser handlers and line/column checks
--SKIPIF--
<?php
if (!function_exists('xml_parser_create')) { echo "skip: xml not available\n"; }
?>
--FILE--
<?php
$xml = "<root>\n  <child attr=\"x\">value</child>\n</root>";
$parser = xml_parser_create();
xml_set_element_handler($parser,
    function($parser,$name){
        echo 'line=' . xml_get_current_line_number($parser) . "\n";
        echo 'col=' . xml_get_current_column_number($parser) . "\n";
    },
    function($parser,$name){
        // no-op
    }
);
// Try to parse and capture error code
$ok = xml_parse($parser,$xml,true);
echo ($ok ? 'ok' : 'err') . "\n";
if (!$ok) {
    echo xml_error_string(xml_get_error_code($parser)) . '\n';
}
xml_parser_free($parser);
?>
--EXPECTF--
line=%d
col=%d
line=%d
col=%d
ok
