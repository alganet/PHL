--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
What php CALLS the token a syntax error stumbled on
--DESCRIPTION--
php names the offending token by KIND before it quotes it, and the vocabulary is
not the type-name one: a stray float literal is a `floating-point number` (where
`float` is what the CAST is called), and a string literal is named by the quote it
was written with, with the SOURCE bytes between the quotes -- escapes unresolved.

Two of them are not a quoted value at all. A double-quoted string that
INTERPOLATES is not one token in php: its scanner emits the opening quote on its
own, so the parser has nothing to quote and reports the bare `double-quote mark`.
And a heredoc is named by its OPENING marker -- `<<<EOT`, or `<<<'EOT` for a
nowdoc, the closing quote dropped because php's token text ends at the label.

`(real)`, removed in php 8, is refused by the SCANNER rather than by the parser,
which is why it reports the same sentence from a position where the token could
never be an operator (`strlen(real)`).

The messages are read through eval(), whose ParseError carries the compiler's
first sentence in both engines. The LINE is left out: php numbers an eval'd chunk
from its own first line and PHL reports the eval() call site, a separate
divergence this test is not about.
--FILE--
<?php
$vocabRows = [
    'integer'             => 'echo 1 2;',
    'float'               => 'echo 1 1.5;',
    'float, leading dot'  => 'echo 1 .5;',
    'float, exponent'     => 'echo 1 1e3;',
    'hex integer'         => 'echo 1 0x1f;',
    'single-quoted'       => "echo 1 'x';",
    'single-quoted, escaped quote' => "echo 1 'a\\'b';",
    'double-quoted'       => 'echo 1 "x";',
    'double-quoted, empty' => 'echo 1 "";',
    'double-quoted, escapes kept' => 'echo 1 "a\\nb";',
    'double-quoted, escaped dollar' => 'echo 1 "a\\$b";',
    'double-quoted, lone dollar' => 'echo 1 "$";',
    'interpolating'       => 'echo 1 "a$b";',
    'interpolating, braced' => 'echo 1 "a{$b}c";',
    'heredoc'             => "echo 1 <<<EOT\nbody\nEOT;\n",
    'nowdoc'              => "echo 1 <<<'EOT'\nbody\nEOT;\n",
    'heredoc, quoted label' => "echo 1 <<<\"EOT\"\nbody\nEOT;\n",
    'heredoc, blanks'     => "echo 1 <<<   EOT\nbody\nEOT;\n",
    'identifier'          => 'echo 1 foo;',
    'variable'            => 'echo 1 $x;',
    'reserved word'       => 'echo 1 class;',
    'removed cast'        => '$b = 1; $a = (real) $b;',
    'removed cast, never an operator' => 'echo strlen(real);',
    'cast holds no newline' => "\$b = 1; \$a = (\nint\n) \$b;",
];
foreach ($vocabRows as $vocabLabel => $vocabSrc) {
    try {
        eval($vocabSrc);
        printf("%-32s ran\n", $vocabLabel);
    } catch (ParseError $vocabErr) {
        printf("%-32s %s\n", $vocabLabel, $vocabErr->getMessage());
    }
}
?>
--EXPECT--
integer                          syntax error, unexpected integer "2", expecting "," or ";"
float                            syntax error, unexpected floating-point number "1.5", expecting "," or ";"
float, leading dot               syntax error, unexpected floating-point number ".5", expecting "," or ";"
float, exponent                  syntax error, unexpected floating-point number "1e3", expecting "," or ";"
hex integer                      syntax error, unexpected integer "0x1f", expecting "," or ";"
single-quoted                    syntax error, unexpected single-quoted string "x", expecting "," or ";"
single-quoted, escaped quote     syntax error, unexpected single-quoted string "a\'b", expecting "," or ";"
double-quoted                    syntax error, unexpected double-quoted string "x", expecting "," or ";"
double-quoted, empty             syntax error, unexpected double-quoted string "", expecting "," or ";"
double-quoted, escapes kept      syntax error, unexpected double-quoted string "a\nb", expecting "," or ";"
double-quoted, escaped dollar    syntax error, unexpected double-quoted string "a\$b", expecting "," or ";"
double-quoted, lone dollar       syntax error, unexpected double-quoted string "$", expecting "," or ";"
interpolating                    syntax error, unexpected double-quote mark, expecting "," or ";"
interpolating, braced            syntax error, unexpected double-quote mark, expecting "," or ";"
heredoc                          syntax error, unexpected heredoc start "<<<EOT", expecting "," or ";"
nowdoc                           syntax error, unexpected heredoc start "<<<'EOT", expecting "," or ";"
heredoc, quoted label            syntax error, unexpected heredoc start "<<<"EOT", expecting "," or ";"
heredoc, blanks                  syntax error, unexpected heredoc start "<<<   EOT", expecting "," or ";"
identifier                       syntax error, unexpected identifier "foo", expecting "," or ";"
variable                         syntax error, unexpected variable "$x", expecting "," or ";"
reserved word                    syntax error, unexpected token "class", expecting "," or ";"
removed cast                     The (real) cast has been removed, use (float) instead
removed cast, never an operator  The (real) cast has been removed, use (float) instead
cast holds no newline            syntax error, unexpected variable "$b"
