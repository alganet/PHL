--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Lexer handles heredoc with UTF-8 characters in delimiter identifier

--FILE--
<?php
// Test heredoc/nowdoc with UTF-8 in delimiter (lex.c lines 821-828)
// The lexer should handle multi-byte UTF-8 characters in heredoc delimiter names

$text = <<<DÉLIMITEUR
This is heredoc content
with UTF-8 delimiter name
DÉLIMITEUR;

echo "Has content: " . (strpos($text, "heredoc") !== false ? "OK" : "FAIL") . "\n";

$text2 = <<<ТЕКСТ
Cyrillic delimiter test
Multiple lines here
ТЕКСТ;

echo "Has cyrillic: " . (strpos($text2, "Cyrillic") !== false ? "OK" : "FAIL") . "\n";

// Nowdoc with UTF-8 delimiter
$text3 = <<<'NÖWDOC'
Nowdoc with umlauts
in delimiter name
NÖWDOC;

echo "Has nowdoc: " . (strpos($text3, "Nowdoc") !== false ? "OK" : "FAIL") . "\n";

// Mixed ASCII and UTF-8 in delimiter
$text4 = <<<TEST_ÜMLÄUT_END
Mixed ASCII and UTF-8 delimiter
TEST_ÜMLÄUT_END;

echo "Has mixed: " . (strpos($text4, "Mixed") !== false ? "OK" : "FAIL") . "\n";

echo "Done\n";
?>
--EXPECT--
Has content: OK
Has cyrillic: OK
Has nowdoc: OK
Has mixed: OK
Done
--CLEAN--
<?php
unset($text, $text2, $text3, $text4);
