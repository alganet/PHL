--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: soundex basic functionality
--FILE--
<?php
// Test basic soundex functionality
echo soundex("Euler") === "E460" ? "EULER_OK\n" : "EULER_FAIL: " . soundex("Euler") . "\n";
echo soundex("Gauss") === "G200" ? "GAUSS_OK\n" : "GAUSS_FAIL: " . soundex("Gauss") . "\n";
echo soundex("Hilbert") === "H416" ? "HILBERT_OK\n" : "HILBERT_FAIL: " . soundex("Hilbert") . "\n";
echo soundex("Knuth") === "K530" ? "KNUTH_OK\n" : "KNUTH_FAIL: " . soundex("Knuth") . "\n";
echo soundex("Lloyd") === "L300" ? "LLOYD_OK\n" : "LLOYD_FAIL: " . soundex("Lloyd") . "\n";
echo soundex("Lukasiewicz") === "L222" ? "LUKASIEWICZ_OK\n" : "LUKASIEWICZ_FAIL: " . soundex("Lukasiewicz") . "\n";

// Test case insensitivity
echo soundex("Euler") === soundex("euler") ? "CASE_INSENSITIVE_OK\n" : "CASE_INSENSITIVE_FAIL\n";

?>
--EXPECT--
EULER_OK
GAUSS_OK
HILBERT_OK
KNUTH_OK
LLOYD_OK
LUKASIEWICZ_OK
CASE_INSENSITIVE_OK
--CLEAN--
<?php

