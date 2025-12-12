--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_error_string returns error string
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip Test requires PH7 XML parser, not PHP's XML extension";
}
?>
--FILE--
<?php
$parser = xml_parser_create();
$xml = '<root><unclosed>';
xml_parse($parser, $xml, true);
$code = xml_get_error_code($parser);
$str = xml_error_string($code);
echo "code=" . $code . "\n";
echo "str_ok=" . (is_string($str) ? 'true' : 'false') . "\n";
xml_parser_free($parser);
?>
--EXPECTF--
code=%d
str_ok=true

