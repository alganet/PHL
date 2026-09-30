--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
token_get_all(): the binary-string prefix, the offset radixes, private(set), (void), a nullsafe interpolation, the close tag's newline and the separator rule
--FILE--
<?php
// One dump per snippet: php's token stream IS the contract a static-analysis
// tool reads, so the id, the text and the line all have to match.
$tlx_snips = [
    'b-prefix'        => "b'foo'; B'foo'; b\"foo\"; B\"foo\"; b\"x\$v\"; b''; bb'foo';",
    'b-heredoc'       => "b<<<'S'\nS;\nB<<<S\nS;\nb <<<'S'\nS;",
    'offset-radix'    => "\"\$a[0]\"; \"\$a[00]\"; \"\$a[0x0]\"; \"\$a[0b1]\"; \"\$a[0o7]\"; \"\$a[1_0]\";",
    'offset-signs'    => "\"\$a[-1]\"; \"\$a[+1]\"; \"\$a[-]\"; \"\$a[0x]\"; \"\$a[1a]\";",
    'offset-giveback' => "\"\$a[ 0]\"; \"\$a[- 1]\"; \"\$a[\$b[0]]\";",
    'asym-visibility' => "class T { private(set) int \$x; PROTECTED(SET) \$y; public(set) \$z; private (set) \$w; }",
    'void-cast'       => "(void) f(); ( void )\$x; (\tvoid\t)\$y; (VOID)\$z; (voidx)\$q;",
    'nullsafe-string' => "\"\$a?->b\"; \"\$a?->b c\"; \"{\$a?->b}\"; <<<S\n\$a?->b\nS;",
    'enum-contextual' => "enum Foo {} class Enum extends X {} class Enum implements Y {} enum Bar: string {}",
    'num-separators'  => "100_; 1__1; 1_.0; 1._0; 0x_1; 0b_1; 1_e2; 1e_2; 1_0.0_1e1_0; 0x7AFE_F00D;",
    // A `b` right after '->'/'?->' is a MEMBER NAME, never a prefix; after '::'
    // it is a prefix again, because that is not php's property state.
    'b-not-a-prefix'  => "\$o->b'x'; \$o?->b\"x\"; C::b'x'; \$o->B<<<'S'\nS;",
];
foreach ($tlx_snips as $tlx_k => $tlx_s) {
    echo "== $tlx_k\n";
    foreach (@token_get_all("<?php " . $tlx_s) as $tlx_t) {
        if (is_array($tlx_t)) {
            printf("  %-28s %d %s\n", token_name($tlx_t[0]), $tlx_t[2], bin2hex($tlx_t[1]));
        } else {
            printf("  %-28s - %s\n", 'CHAR', bin2hex($tlx_t));
        }
    }
}
// The close tag owns one newline, and a lone CR is one.
foreach (["?>\ntail", "?>\r\ntail", "?>\rtail", "?>\r\r\ntail", "?>tail"] as $tlx_i => $tlx_s) {
    echo "== closetag$tlx_i\n";
    foreach (@token_get_all("<?php echo 1;\n" . $tlx_s) as $tlx_t) {
        if (is_array($tlx_t)) printf("  %-28s %d %s\n", token_name($tlx_t[0]), $tlx_t[2], bin2hex($tlx_t[1]));
        else printf("  %-28s - %s\n", 'CHAR', bin2hex($tlx_t));
    }
}
// __halt_compiler's terminator may be a close tag, and the remainder keeps the
// tag's line rather than the one after it.
foreach (["__halt_compiler();tail", "__halt_compiler()\n?>\ntail", "__halt_compiler()?>tail"] as $tlx_i => $tlx_s) {
    echo "== halt$tlx_i\n";
    foreach (@token_get_all("<?php " . $tlx_s) as $tlx_t) {
        if (is_array($tlx_t)) printf("  %-28s %d %s\n", token_name($tlx_t[0]), $tlx_t[2], bin2hex($tlx_t[1]));
        else printf("  %-28s - %s\n", 'CHAR', bin2hex($tlx_t));
    }
}
// PhpToken::tokenize rides the same scan; its id for a single-character token is
// the character itself, which for the two-byte `b"` opener is the QUOTE.
foreach (@PhpToken::tokenize('<?php b"x$v"; b\'y\';') as $tlx_t) {
    /* a T_* id is the build's parser numbering (php's Windows build differs);
     * a single-character token's id is the character, and that is php's */
    printf("  %-28s %s %d %s\n", $tlx_t->getTokenName(), $tlx_t->id < 256 ? $tlx_t->id : 'T', $tlx_t->pos, bin2hex($tlx_t->text));
}
?>
--EXPECT--
== b-prefix
  T_OPEN_TAG                   1 3c3f70687020
  T_CONSTANT_ENCAPSED_STRING   1 6227666f6f27
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_CONSTANT_ENCAPSED_STRING   1 4227666f6f27
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_CONSTANT_ENCAPSED_STRING   1 6222666f6f22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_CONSTANT_ENCAPSED_STRING   1 4222666f6f22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 6222
  T_ENCAPSED_AND_WHITESPACE    1 78
  T_VARIABLE                   1 2476
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_CONSTANT_ENCAPSED_STRING   1 622727
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_STRING                     1 6262
  T_CONSTANT_ENCAPSED_STRING   1 27666f6f27
  CHAR                         - 3b
== b-heredoc
  T_OPEN_TAG                   1 3c3f70687020
  T_START_HEREDOC              1 623c3c3c2753270a
  T_END_HEREDOC                2 53
  CHAR                         - 3b
  T_WHITESPACE                 2 0a
  T_START_HEREDOC              3 423c3c3c530a
  T_END_HEREDOC                4 53
  CHAR                         - 3b
  T_WHITESPACE                 4 0a
  T_STRING                     5 62
  T_WHITESPACE                 5 20
  T_START_HEREDOC              5 3c3c3c2753270a
  T_END_HEREDOC                6 53
  CHAR                         - 3b
== offset-radix
  T_OPEN_TAG                   1 3c3f70687020
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_NUM_STRING                 1 30
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_NUM_STRING                 1 3030
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_NUM_STRING                 1 307830
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_NUM_STRING                 1 306231
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_NUM_STRING                 1 306f37
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_NUM_STRING                 1 315f30
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
== offset-signs
  T_OPEN_TAG                   1 3c3f70687020
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  CHAR                         - 2d
  T_NUM_STRING                 1 31
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  CHAR                         - 2b
  T_NUM_STRING                 1 31
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  CHAR                         - 2d
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_NUM_STRING                 1 30
  T_STRING                     1 78
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_NUM_STRING                 1 31
  T_STRING                     1 61
  CHAR                         - 5d
  CHAR                         - 22
  CHAR                         - 3b
== offset-giveback
  T_OPEN_TAG                   1 3c3f70687020
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_ENCAPSED_AND_WHITESPACE    1 
  T_ENCAPSED_AND_WHITESPACE    1 20305d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  CHAR                         - 2d
  T_ENCAPSED_AND_WHITESPACE    1 
  T_ENCAPSED_AND_WHITESPACE    1 20315d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  CHAR                         - 5b
  T_VARIABLE                   1 2462
  CHAR                         - 5b
  T_NUM_STRING                 1 30
  CHAR                         - 5d
  T_ENCAPSED_AND_WHITESPACE    1 5d
  CHAR                         - 22
  CHAR                         - 3b
== asym-visibility
  T_OPEN_TAG                   1 3c3f70687020
  T_CLASS                      1 636c617373
  T_WHITESPACE                 1 20
  T_STRING                     1 54
  T_WHITESPACE                 1 20
  CHAR                         - 7b
  T_WHITESPACE                 1 20
  T_PRIVATE_SET                1 707269766174652873657429
  T_WHITESPACE                 1 20
  T_STRING                     1 696e74
  T_WHITESPACE                 1 20
  T_VARIABLE                   1 2478
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_PROTECTED_SET              1 50524f5445435445442853455429
  T_WHITESPACE                 1 20
  T_VARIABLE                   1 2479
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_PUBLIC_SET                 1 7075626c69632873657429
  T_WHITESPACE                 1 20
  T_VARIABLE                   1 247a
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_PRIVATE                    1 70726976617465
  T_WHITESPACE                 1 20
  CHAR                         - 28
  T_STRING                     1 736574
  CHAR                         - 29
  T_WHITESPACE                 1 20
  T_VARIABLE                   1 2477
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 7d
== void-cast
  T_OPEN_TAG                   1 3c3f70687020
  T_VOID_CAST                  1 28766f696429
  T_WHITESPACE                 1 20
  T_STRING                     1 66
  CHAR                         - 28
  CHAR                         - 29
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_VOID_CAST                  1 2820766f69642029
  T_VARIABLE                   1 2478
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_VOID_CAST                  1 2809766f69640929
  T_VARIABLE                   1 2479
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_VOID_CAST                  1 28564f494429
  T_VARIABLE                   1 247a
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 28
  T_STRING                     1 766f696478
  CHAR                         - 29
  T_VARIABLE                   1 2471
  CHAR                         - 3b
== nullsafe-string
  T_OPEN_TAG                   1 3c3f70687020
  CHAR                         - 22
  T_VARIABLE                   1 2461
  T_NULLSAFE_OBJECT_OPERATOR   1 3f2d3e
  T_STRING                     1 62
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_VARIABLE                   1 2461
  T_NULLSAFE_OBJECT_OPERATOR   1 3f2d3e
  T_STRING                     1 62
  T_ENCAPSED_AND_WHITESPACE    1 2063
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  CHAR                         - 22
  T_CURLY_OPEN                 1 7b
  T_VARIABLE                   1 2461
  T_NULLSAFE_OBJECT_OPERATOR   1 3f2d3e
  T_STRING                     1 62
  CHAR                         - 7d
  CHAR                         - 22
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_START_HEREDOC              1 3c3c3c530a
  T_VARIABLE                   2 2461
  T_NULLSAFE_OBJECT_OPERATOR   2 3f2d3e
  T_STRING                     2 62
  T_ENCAPSED_AND_WHITESPACE    2 0a
  T_END_HEREDOC                3 53
  CHAR                         - 3b
== enum-contextual
  T_OPEN_TAG                   1 3c3f70687020
  T_ENUM                       1 656e756d
  T_WHITESPACE                 1 20
  T_STRING                     1 466f6f
  T_WHITESPACE                 1 20
  CHAR                         - 7b
  CHAR                         - 7d
  T_WHITESPACE                 1 20
  T_CLASS                      1 636c617373
  T_WHITESPACE                 1 20
  T_STRING                     1 456e756d
  T_WHITESPACE                 1 20
  T_EXTENDS                    1 657874656e6473
  T_WHITESPACE                 1 20
  T_STRING                     1 58
  T_WHITESPACE                 1 20
  CHAR                         - 7b
  CHAR                         - 7d
  T_WHITESPACE                 1 20
  T_CLASS                      1 636c617373
  T_WHITESPACE                 1 20
  T_STRING                     1 456e756d
  T_WHITESPACE                 1 20
  T_IMPLEMENTS                 1 696d706c656d656e7473
  T_WHITESPACE                 1 20
  T_STRING                     1 59
  T_WHITESPACE                 1 20
  CHAR                         - 7b
  CHAR                         - 7d
  T_WHITESPACE                 1 20
  T_ENUM                       1 656e756d
  T_WHITESPACE                 1 20
  T_STRING                     1 426172
  CHAR                         - 3a
  T_WHITESPACE                 1 20
  T_STRING                     1 737472696e67
  T_WHITESPACE                 1 20
  CHAR                         - 7b
  CHAR                         - 7d
== num-separators
  T_OPEN_TAG                   1 3c3f70687020
  T_LNUMBER                    1 313030
  T_STRING                     1 5f
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 31
  T_STRING                     1 5f5f31
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 31
  T_STRING                     1 5f
  T_DNUMBER                    1 2e30
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_DNUMBER                    1 312e
  T_STRING                     1 5f30
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 30
  T_STRING                     1 785f31
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 30
  T_STRING                     1 625f31
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 31
  T_STRING                     1 5f6532
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 31
  T_STRING                     1 655f32
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_DNUMBER                    1 315f302e305f3165315f30
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 3078374146455f46303044
  CHAR                         - 3b
== b-not-a-prefix
  T_OPEN_TAG                   1 3c3f70687020
  T_VARIABLE                   1 246f
  T_OBJECT_OPERATOR            1 2d3e
  T_STRING                     1 62
  T_CONSTANT_ENCAPSED_STRING   1 277827
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_VARIABLE                   1 246f
  T_NULLSAFE_OBJECT_OPERATOR   1 3f2d3e
  T_STRING                     1 62
  T_CONSTANT_ENCAPSED_STRING   1 227822
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_STRING                     1 43
  T_DOUBLE_COLON               1 3a3a
  T_CONSTANT_ENCAPSED_STRING   1 62277827
  CHAR                         - 3b
  T_WHITESPACE                 1 20
  T_VARIABLE                   1 246f
  T_OBJECT_OPERATOR            1 2d3e
  T_STRING                     1 42
  T_START_HEREDOC              1 3c3c3c2753270a
  T_END_HEREDOC                2 53
  CHAR                         - 3b
== closetag0
  T_OPEN_TAG                   1 3c3f70687020
  T_ECHO                       1 6563686f
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 31
  CHAR                         - 3b
  T_WHITESPACE                 1 0a
  T_CLOSE_TAG                  2 3f3e0a
  T_INLINE_HTML                3 7461696c
== closetag1
  T_OPEN_TAG                   1 3c3f70687020
  T_ECHO                       1 6563686f
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 31
  CHAR                         - 3b
  T_WHITESPACE                 1 0a
  T_CLOSE_TAG                  2 3f3e0d0a
  T_INLINE_HTML                3 7461696c
== closetag2
  T_OPEN_TAG                   1 3c3f70687020
  T_ECHO                       1 6563686f
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 31
  CHAR                         - 3b
  T_WHITESPACE                 1 0a
  T_CLOSE_TAG                  2 3f3e0d
  T_INLINE_HTML                3 7461696c
== closetag3
  T_OPEN_TAG                   1 3c3f70687020
  T_ECHO                       1 6563686f
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 31
  CHAR                         - 3b
  T_WHITESPACE                 1 0a
  T_CLOSE_TAG                  2 3f3e0d
  T_INLINE_HTML                3 0d0a7461696c
== closetag4
  T_OPEN_TAG                   1 3c3f70687020
  T_ECHO                       1 6563686f
  T_WHITESPACE                 1 20
  T_LNUMBER                    1 31
  CHAR                         - 3b
  T_WHITESPACE                 1 0a
  T_CLOSE_TAG                  2 3f3e
  T_INLINE_HTML                2 7461696c
== halt0
  T_OPEN_TAG                   1 3c3f70687020
  T_HALT_COMPILER              1 5f5f68616c745f636f6d70696c6572
  CHAR                         - 28
  CHAR                         - 29
  CHAR                         - 3b
  T_INLINE_HTML                1 7461696c
== halt1
  T_OPEN_TAG                   1 3c3f70687020
  T_HALT_COMPILER              1 5f5f68616c745f636f6d70696c6572
  CHAR                         - 28
  CHAR                         - 29
  T_WHITESPACE                 1 0a
  T_CLOSE_TAG                  2 3f3e0a
  T_INLINE_HTML                2 7461696c
== halt2
  T_OPEN_TAG                   1 3c3f70687020
  T_HALT_COMPILER              1 5f5f68616c745f636f6d70696c6572
  CHAR                         - 28
  CHAR                         - 29
  T_CLOSE_TAG                  1 3f3e
  T_INLINE_HTML                1 7461696c
  T_OPEN_TAG                   T 0 3c3f70687020
  "                            34 6 6222
  T_ENCAPSED_AND_WHITESPACE    T 8 78
  T_VARIABLE                   T 9 2476
  "                            34 11 22
  ;                            59 12 3b
  T_WHITESPACE                 T 13 20
  T_CONSTANT_ENCAPSED_STRING   T 14 62277927
  ;                            59 18 3b
