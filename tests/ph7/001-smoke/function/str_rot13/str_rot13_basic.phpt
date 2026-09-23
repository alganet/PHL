--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_rot13 rotates ASCII letters by 13 and leaves everything else alone
--FILE--
<?php
// Letters rotate 13 places in their own case; everything else passes through.
var_dump(str_rot13("Hello, World! abc XYZ 123"));
var_dump(str_rot13("nowhere"));
var_dump(str_rot13(str_rot13("round trip is the identity")));
var_dump(str_rot13(""));
// Non-letters (digits, punctuation, high bytes) are untouched.
var_dump(bin2hex(str_rot13("a\xc3\xa9z")));
?>
--EXPECT--
string(25) "Uryyb, Jbeyq! nop KLM 123"
string(7) "abjurer"
string(26) "round trip is the identity"
string(0) ""
string(8) "6ec3a96d"
