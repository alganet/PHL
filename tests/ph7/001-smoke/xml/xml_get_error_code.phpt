--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_get_error_code returns error code
--FILE--
<?php
$parser = xml_parser_create();
$xml = '<root><unclosed>';
$result = xml_parse($parser, $xml, true);
$code = xml_get_error_code($parser);
echo "code=" . $code . "\n";
echo "code_ok=" . ($code > 0 ? 'true' : 'false') . "\n";
xml_parser_free($parser);
?>
--EXPECTF--
code=%d
code_ok=true
--CLEAN--
<?php
unset($parser, $xml, $result, $code);
