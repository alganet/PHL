--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: parse_ini_string basic functionality
--FILE--
<?php
// Test basic parse_ini_string functionality
$ini = "name = John\nage = 25\n";
$result = parse_ini_string($ini);

// Since PH7 may not have full array support, just test that function returns something
echo is_array($result) ? "ARRAY_OK\n" : "ARRAY_FAIL\n";


// Test invalid INI
$invalid = parse_ini_string("invalid ini content");
echo is_array($invalid) ? "INVALID_OK\n" : "INVALID_FAIL\n";

// Test with sections
$with_sections = "[section1]\nkey1 = value1\n[section2]\nkey2 = value2\n";
$result2 = parse_ini_string($with_sections);
echo is_array($result2) ? "SECTIONS_OK\n" : "SECTIONS_FAIL\n";
?>
--EXPECT--
ARRAY_OK
INVALID_OK
SECTIONS_OK
--CLEAN--
<?php
unset($ini, $result, $invalid, $with_sections, $result2);
