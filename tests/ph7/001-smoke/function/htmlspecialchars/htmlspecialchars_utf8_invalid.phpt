--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars invalid UTF-8: substitute (default) / ignore / reject-to-empty
--FILE--
<?php
echo bin2hex(htmlspecialchars("a\xE9b")), "\n";
echo bin2hex(htmlspecialchars("a\xE9b", ENT_QUOTES | ENT_IGNORE)), "\n";
echo bin2hex(htmlspecialchars("a\xE9b", ENT_QUOTES)), "\n";
echo bin2hex(htmlspecialchars("é<a>")), "\n";                    // valid UTF-8 untouched
?>
--EXPECT--
61efbfbd62
6162

c3a9266c743b612667743b
--CLEAN--
<?php
