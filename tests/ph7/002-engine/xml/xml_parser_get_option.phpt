--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_parser_get_option gets parser option
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip Test requires PH7 XML parser, not PHP's XML extension";
}
?>
--FILE--
<?php
$parser = xml_parser_create();
$result = xml_parser_get_option($parser, 1);
echo "get_option=" . ($result !== false ? 'value' : 'false') . "\n";
xml_parser_free($parser);
?>
--EXPECT--
get_option=false

