--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
escapeshellarg()/escapeshellcmd() on a POSIX shell
--SKIPIF--
<?php
if (PHP_OS_FAMILY === 'Windows') {
    echo "skip both escapers are platform-specific in php (single quotes and backslash escapes here, cmd.exe quoting and ^ there); the Windows half is escapeshellarg_windows.phpt";
}
?>
--FILE--
<?php
/* php quotes the whole argument with a character the shell does not look inside,
 * and the only byte that could end the quoting is rewritten: `'` becomes `'\''`,
 * i.e. close, escape, reopen. Nothing else is touched -- $, `, ;, * and a
 * newline are all inert between single quotes. */
$esa_cases = [
    'empty'        => '',
    'plain'        => 'file.txt',
    'space'        => 'two words',
    'quote'        => "it's",
    'quotes'       => "a'b'c",
    'dollar'       => '$HOME',
    'backtick'     => '`id`',
    'semicolon'    => 'x; rm -rf /',
    'newline'      => "a\nb",
    'backslash'    => 'a\\b',
    'dquote'       => '"q"',
    'star'         => '*',
    'only quote'   => "'",
];
foreach ($esa_cases as $esa_name => $esa_s) {
    printf("%-12s %s\n", $esa_name, var_export(escapeshellarg($esa_s), true));
}

/* escapeshellcmd() escapes a COMMAND, so there is no quoting to hide behind:
 * every byte that could break out of one is backslash-escaped where it stands.
 * The exception is a quote php can PAIR -- the command may legitimately quote
 * one of its own arguments -- so only an unpaired one is escaped. */
echo "== cmd ==\n";
$esc_cases = [
    'empty'        => '',
    'plain'        => 'ls -l /tmp',
    'amp'          => 'a&b',
    'semicolon'    => 'echo hi; rm -rf /',
    'pipe'         => 'a|b',
    'backtick'     => 'a`id`b',
    'glob'         => 'a*b?c',
    'tilde'        => '~/x',
    'redirect'     => 'a<b>c',
    'caret'        => 'a^b',
    'parens'       => 'a(b)c',
    'brackets'     => 'a[b]c',
    'braces'       => 'a{b}c',
    'dollar'       => 'a$b',
    'backslash'    => 'a\\b',
    'newline'      => "a\nb",
    'hash'         => 'a#b',
    'squote pair'  => "echo 'hi there'",
    'squote odd'   => "echo 'hi",
    'dquote pair'  => 'echo "hi there"',
    'dquote odd'   => 'echo "hi',
    'mixed quotes' => "a'b\"c'd",
    'three squote' => "a'b'c'd",
    'not escaped'  => 'a!b%c,d:e=f+g-h_i',
];
foreach ($esc_cases as $esc_name => $esc_s) {
    printf("%-13s %s\n", $esc_name, var_export(escapeshellcmd($esc_s), true));
}
?>
--EXPECT--
empty        '\'\''
plain        '\'file.txt\''
space        '\'two words\''
quote        '\'it\'\\\'\'s\''
quotes       '\'a\'\\\'\'b\'\\\'\'c\''
dollar       '\'$HOME\''
backtick     '\'`id`\''
semicolon    '\'x; rm -rf /\''
newline      '\'a
b\''
backslash    '\'a\\b\''
dquote       '\'"q"\''
star         '\'*\''
only quote   '\'\'\\\'\'\''
== cmd ==
empty         ''
plain         'ls -l /tmp'
amp           'a\\&b'
semicolon     'echo hi\\; rm -rf /'
pipe          'a\\|b'
backtick      'a\\`id\\`b'
glob          'a\\*b\\?c'
tilde         '\\~/x'
redirect      'a\\<b\\>c'
caret         'a\\^b'
parens        'a\\(b\\)c'
brackets      'a\\[b\\]c'
braces        'a\\{b\\}c'
dollar        'a\\$b'
backslash     'a\\\\b'
newline       'a\\
b'
hash          'a\\#b'
squote pair   'echo \'hi there\''
squote odd    'echo \\\'hi'
dquote pair   'echo "hi there"'
dquote odd    'echo \\"hi'
mixed quotes  'a\'b\\"c\'d'
three squote  'a\'b\'c\\\'d'
not escaped   'a!b%c,d:e=f+g-h_i'
--CLEAN--
<?php
unset($esa_cases, $esa_name, $esa_s, $esc_cases, $esc_name, $esc_s);
