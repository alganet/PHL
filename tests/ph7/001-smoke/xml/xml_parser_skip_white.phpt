--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_parser_set_option with XML_OPTION_SKIP_WHITE
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$parser = xml_parser_create();
xml_parser_set_option($parser, XML_OPTION_SKIP_WHITE, 1);
xml_parse($parser, "<root></root>", true);
xml_parser_free($parser);
echo "OK";
?>
--EXPECTF--
Error [8192]: Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0 in %s on line %d
OK
--CLEAN--
<?php
unset($parser);
