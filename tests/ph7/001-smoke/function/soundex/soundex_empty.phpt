--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
soundex returns "0000" for input with no alphabetic character
--FILE--
<?php
var_dump(soundex(""));
var_dump(soundex("123"));
var_dump(soundex("!!!"));
?>
--EXPECT--
string(4) "0000"
string(4) "0000"
string(4) "0000"
--CLEAN--
<?php

