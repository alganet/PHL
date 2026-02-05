--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml processing instruction incomplete
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$parser = xml_parser_create();
xml_set_processing_instruction_handler($parser, 'pi_handler');
$xml = chr(60) . chr(63) . 'target data';
$result = xml_parse($parser, $xml, true);
xml_parser_free($parser);

echo "Parse result: $result\n";
function pi_handler($parser, $target, $data){
    echo "PI: $target $data\n";
}
?>
--EXPECT--
Parse result: 0
--CLEAN--
<?php
unset($parser, $xml, $result);
