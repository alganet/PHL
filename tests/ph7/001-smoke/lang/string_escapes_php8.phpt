--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP-8-exact double-quoted/heredoc escape semantics (PH7-isms removed)
--FILE--
<?php
// Replaces the PH7-era escape corpus: the \oNNN octal form is gone, unknown
// escapes keep their backslash (php has no \a, \b or \' in double quotes),
// and bare octal \NNN, \e and \u{...} now work. Byte-compared against php.

// Recognized single-character escapes
echo "codes: ", ord("\n"), " ", ord("\r"), " ", ord("\t"), " ", ord("\v"), " ", ord("\f"), " ", ord("\e"), "\n";
echo "dollar-quote-backslash: ", "\$x", " ", "\"q\"", " ", bin2hex("\\"), "\n";

// Not escapes in php: backslash is preserved
echo "not-escapes: ", bin2hex("\a"), " ", bin2hex("\b"), " ", bin2hex("\'"), "\n";
echo "unknown: ", bin2hex("\d"), " ", bin2hex("\q"), " ", bin2hex("\z"), "\n";
echo "o-literal: ", "\o146", " ", "\o", " ", "\o9", "\n";

// Bare octal \[0-7]{1,3}
echo "octal: ", bin2hex("\0"), " ", bin2hex("\1"), " ", bin2hex("\12"), " ", "\101\102", " ", bin2hex("\377"), " ", "\1018", "\n";

// Hex \x with 1-2 digits; \x before a non-hex char keeps the backslash
echo "hex: ", "\x41", " ", bin2hex("\x5"), " ", bin2hex("\x5!"), " ", bin2hex("\xG"), " ", bin2hex("\x"), "\n";

// Unicode codepoint escapes \u{...}; bare \u stays literal
echo "unicode: ", bin2hex("\u{e9}"), " ", bin2hex("\u{1F600}"), " ", "\u{0000041}", " ", bin2hex("\u"), " ", bin2hex("\u9"), "\n";

// Heredoc shares double-quote escape semantics — except \" (no active quote
// char, so the backslash stays) — and keeps a trailing lone backslash
$v = "interp";
echo <<<EOT
heredoc: \o146 \d \146 \u{e9} \x41 $v \"q\" \$raw tail\
EOT;
echo "\n";

// \u{ followed by $ is not a codepoint escape: literal \u + {$...} interpolation
echo "u-interp: ", "\u{$v}", "\n";

// Nowdoc: no escape processing at all
echo <<<'EOT'
nowdoc: \o146 \d \146 \u{e9} \x41 \n
EOT;
echo "\n";
?>
--EXPECT--
codes: 10 13 9 11 12 27
dollar-quote-backslash: $x "q" 5c
not-escapes: 5c61 5c62 5c27
unknown: 5c64 5c71 5c7a
o-literal: \o146 \o \o9
octal: 00 01 0a AB ff A8
hex: A 05 0521 5c7847 5c78
unicode: c3a9 f09f9880 A 5c75 5c7539
heredoc: \o146 \d f é A interp \"q\" $raw tail\
u-interp: \uinterp
nowdoc: \o146 \d \146 \u{e9} \x41 \n
--CLEAN--
<?php
unset($v);
