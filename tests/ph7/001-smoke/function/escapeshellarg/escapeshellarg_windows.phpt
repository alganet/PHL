--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
escapeshellarg()/escapeshellcmd() on cmd.exe
--SKIPIF--
<?php
if (PHP_OS_FAMILY !== 'Windows') {
    echo "skip both escapers are platform-specific in php; the POSIX half is escapeshellarg_posix.phpt";
}
?>
--FILE--
<?php
/* cmd.exe has no in-quote escape for `"`, so php cannot rewrite one the way the
 * POSIX half rewrites `'` -- it REPLACES it with a space, along with `%` and `!`,
 * which cmd.exe still expands inside double quotes. A trailing ODD run of
 * backslashes is doubled so the last one escapes itself rather than the closing
 * quote. Everything else rides through untouched. */
$esa_cases = [
    'empty'        => '',
    'plain'        => 'file.txt',
    'space'        => 'two words',
    'squote'       => "it's",
    'dquote'       => '"q"',
    'percent'      => '%PATH%',
    'bang'         => 'a!b!',
    'dollar'       => '$HOME',
    'one slash'    => 'a\\',
    'two slashes'  => 'a\\\\',
    'inner slash'  => 'a\\b',
];
foreach ($esa_cases as $esa_name => $esa_s) {
    printf("%-12s %s\n", $esa_name, var_export(escapeshellarg($esa_s), true));
}

/* escapeshellcmd() prefixes cmd.exe's escape character, `^`. There is no
 * quote-PAIRING rule here -- that half of php's switch is POSIX-only -- so both
 * quote characters are ordinary escapes, and so are `%` and `!`. */
echo "== cmd ==\n";
$esc_cases = [
    'empty'        => '',
    'plain'        => 'dir C:\\Temp',
    'amp'          => 'a&b',
    'pipe'         => 'a|b',
    'percent'      => 'echo %PATH%',
    'bang'         => 'echo !x!',
    'squote pair'  => "echo 'hi there'",
    'dquote pair'  => 'echo "hi there"',
    'redirect'     => 'a<b>c',
    'caret'        => 'a^b',
    'not escaped'  => 'a,b:c=d+e-f_g',
];
foreach ($esc_cases as $esc_name => $esc_s) {
    printf("%-13s %s\n", $esc_name, var_export(escapeshellcmd($esc_s), true));
}
?>
--EXPECT--
empty        '""'
plain        '"file.txt"'
space        '"two words"'
squote       '"it\'s"'
dquote       '" q "'
percent      '" PATH "'
bang         '"a b "'
dollar       '"$HOME"'
one slash    '"a\\\\"'
two slashes  '"a\\\\"'
inner slash  '"a\\b"'
== cmd ==
empty         ''
plain         'dir C:^\\Temp'
amp           'a^&b'
pipe          'a^|b'
percent       'echo ^%PATH^%'
bang          'echo ^!x^!'
squote pair   'echo ^\'hi there^\''
dquote pair   'echo ^"hi there^"'
redirect      'a^<b^>c'
caret         'a^^b'
not escaped   'a,b:c=d+e-f_g'
--CLEAN--
<?php
unset($esa_cases, $esa_name, $esa_s, $esc_cases, $esc_name, $esc_s);
