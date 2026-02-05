--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ltrim with custom character list
--FILE--
<?php
// Test ltrim with custom charlist
$str = "xxxHello Worldxxx";
echo "ltrim=BEGIN" . ltrim($str, "x") . "END\n";

// Test with multiple characters in charlist
$str2 = "abcHello Worldabc";
echo "ltrim=BEGIN" . ltrim($str2, "abc") . "END\n";

// Test charlist that trims nothing
$str3 = "Hello World";
echo "ltrim=BEGIN" . ltrim($str3, "xyz") . "END\n";

// Test empty charlist returns string unchanged
$str4 = "  Hello  ";
echo "ltrim=BEGIN" . ltrim($str4, "") . "END\n";
?>
--EXPECT--
ltrim=BEGINHello WorldxxxEND
ltrim=BEGINHello WorldabcEND
ltrim=BEGINHello WorldEND
ltrim=BEGIN  Hello  END
--CLEAN--
<?php
unset($str, $str2, $str3, $str4);
