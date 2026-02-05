--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
urldecode handles UTF-8 encoded characters
--FILE--
<?php
// Test UTF-8 URL decoding
// This tests the UTF-8 decoding path in SyUriDecode
$utf8_encoded = 'caf%C3%A9';  // café in UTF-8
$result = urldecode($utf8_encoded);
echo $result . "\n";

// Test multi-byte UTF-8 sequence
$multi_byte = '%E2%82%AC';  // Euro symbol €
$result2 = urldecode($multi_byte);
echo $result2 . "\n";
?>
--EXPECT--
café
€
--CLEAN--
<?php
unset($utf8_encoded, $result, $multi_byte, $result2);
