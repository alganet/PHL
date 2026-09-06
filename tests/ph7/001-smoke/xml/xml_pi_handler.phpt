--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml processing instruction handler called
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip Test requires PH7 XML parser, not PHP's XML extension";
}
?>
--FILE--
<?php
$parser = xml_parser_create();
xml_set_processing_instruction_handler($parser, 'pi_handler');
$xml = chr(60) . chr(63) . 'target data' . chr(63) . chr(62) . '<root></root>';
$result = xml_parse($parser, $xml, true);
xml_parser_free($parser);

echo "Parse result: $result\n";
function pi_handler($parser, $target, $data){
    echo "PI: $target $data\n";
}
?>
--EXPECTF--
PI: target data
Error [8192]: Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0 in %s on line %d
Parse result: 1
--CLEAN--
<?php
unset($parser, $xml, $result);
