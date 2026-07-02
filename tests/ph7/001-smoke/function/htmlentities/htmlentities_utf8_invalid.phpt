--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlentities invalid UTF-8: U+FFFD per maximal subpart, ENT_IGNORE drops, neither rejects all
--FILE--
<?php
echo bin2hex(htmlentities("a\xE9b")), "\n";                      // default: SUBSTITUTE
echo bin2hex(htmlentities("\x80\xC3\x28")), "\n";                // 2 subparts + "("
echo bin2hex(htmlentities("\xE0\x80\xAF")), "\n";                // ONE maximal subpart
echo bin2hex(htmlentities("a\xE9b", ENT_QUOTES | ENT_IGNORE)), "\n";
echo bin2hex(htmlentities("a\xE9b", ENT_QUOTES)), "\n";          // no SUBSTITUTE: ""
?>
--EXPECT--
61efbfbd62
efbfbdefbfbd28
efbfbd
6162

--CLEAN--
<?php
