--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sscanf() reads php's whole conversion alphabet, quirks included
--FILE--
<?php
/* php's scanf came from Tcl, not from scanf(3), and four of its rules are only
 * visible from outside: `%c` is `%s` with the whitespace skip turned off (so it
 * can answer the EMPTY string), the numeric scanners BACK UP over a `0x` or an
 * `e` whose digits never arrived, `%u` hands back a STRING when the value will
 * not fit a signed int, and a conversion the input cannot satisfy stops the
 * scan and leaves everything after it NULL. */
function sscanf_row(string $subject, string $format): void {
    printf("%-26s %-12s -> %s\n", var_export($subject, true), var_export($format, true),
        str_replace("\n", '', var_export(sscanf($subject, $format), true)));
}
echo "## integers\n";
sscanf_row('age: 25', 'age: %d');
sscanf_row('2024-01-15', '%d-%d-%d');
sscanf_row('+5', '%d');
sscanf_row('- 5', '%d');
sscanf_row('0x1A', '%x');
sscanf_row('0x', '%x');
sscanf_row('0x10', '%d');
sscanf_row('0x10', '%i');
sscanf_row('010', '%i');
sscanf_row('777', '%o');
sscanf_row('12', '%1d%1d');
sscanf_row('99999999999999999999', '%d');
sscanf_row('-99999999999999999999', '%d');
sscanf_row('-5', '%u');
sscanf_row('99999999999999999999', '%u');
sscanf_row('-0', '%u');
sscanf_row('5', '%ld');
sscanf_row('5', '%hd');
sscanf_row('5', '%Ld');

echo "## floats\n";
sscanf_row('12.5', '%f');
sscanf_row('12.5e3', '%e');
sscanf_row('.5', '%f');
sscanf_row('5.', '%f');
sscanf_row('5e', '%f');
sscanf_row('5e+', '%f');
sscanf_row('e5', '%f');
sscanf_row('.', '%f');

echo "## strings, characters and sets\n";
sscanf_row('hello world', '%s %s');
sscanf_row('hello world', '%5s');
sscanf_row('hello', '%c');
sscanf_row('hello', '%3c');
sscanf_row('a b', '%3c');
sscanf_row('  ab', '%c');
sscanf_row('hello', '%0c');
sscanf_row('abc123', '%[a-z]');
sscanf_row('abc123', '%[^0-9]');
sscanf_row('abc123', '%[]a-z]');
sscanf_row(']x', '%[]]');
sscanf_row('abc', '%[a-]');
sscanf_row('-abc', '%[a-c-]');
sscanf_row('a-c', '%[a\-c]');
sscanf_row('a^b', '%[a^b]');
sscanf_row('A', '%[c-a]');
sscanf_row('abcd', '%[^a]');

echo "## the scan's own bookkeeping\n";
sscanf_row('12abc', '%d%n%s');
sscanf_row('100%', '%d%%');
sscanf_row('a b', 'a%nb');
sscanf_row('', '%n');
sscanf_row('hello', '%*s%s');
sscanf_row('hello world', '%*s %s');
sscanf_row('5', '%*d');
sscanf_row('', '%*d');
sscanf_row('5 6 7', '%d %d %d %d');
sscanf_row('1x2', '%d-%d');
sscanf_row('abc', '%d');
sscanf_row('', '%d');
sscanf_row("a\0b", '%s');
sscanf_row('abc', '');
sscanf_row('', 'abc');
--EXPECT--
## integers
'age: 25'                  'age: %d'    -> array (  0 => 25,)
'2024-01-15'               '%d-%d-%d'   -> array (  0 => 2024,  1 => 1,  2 => 15,)
'+5'                       '%d'         -> array (  0 => 5,)
'- 5'                      '%d'         -> array (  0 => NULL,)
'0x1A'                     '%x'         -> array (  0 => 26,)
'0x'                       '%x'         -> array (  0 => 0,)
'0x10'                     '%d'         -> array (  0 => 0,)
'0x10'                     '%i'         -> array (  0 => 16,)
'010'                      '%i'         -> array (  0 => 8,)
'777'                      '%o'         -> array (  0 => 511,)
'12'                       '%1d%1d'     -> array (  0 => 1,  1 => 2,)
'99999999999999999999'     '%d'         -> array (  0 => 9223372036854775807,)
'-99999999999999999999'    '%d'         -> array (  0 => -9223372036854775807-1,)
'-5'                       '%u'         -> array (  0 => '18446744073709551611',)
'99999999999999999999'     '%u'         -> array (  0 => '18446744073709551615',)
'-0'                       '%u'         -> array (  0 => 0,)
'5'                        '%ld'        -> array (  0 => 5,)
'5'                        '%hd'        -> array (  0 => 5,)
'5'                        '%Ld'        -> array (  0 => 5,)
## floats
'12.5'                     '%f'         -> array (  0 => 12.5,)
'12.5e3'                   '%e'         -> array (  0 => 12500.0,)
'.5'                       '%f'         -> array (  0 => 0.5,)
'5.'                       '%f'         -> array (  0 => 5.0,)
'5e'                       '%f'         -> array (  0 => 5.0,)
'5e+'                      '%f'         -> array (  0 => 5.0,)
'e5'                       '%f'         -> array (  0 => NULL,)
'.'                        '%f'         -> NULL
## strings, characters and sets
'hello world'              '%s %s'      -> array (  0 => 'hello',  1 => 'world',)
'hello world'              '%5s'        -> array (  0 => 'hello',)
'hello'                    '%c'         -> array (  0 => 'h',)
'hello'                    '%3c'        -> array (  0 => 'hel',)
'a b'                      '%3c'        -> array (  0 => 'a',)
'  ab'                     '%c'         -> array (  0 => '',)
'hello'                    '%0c'        -> array (  0 => 'h',)
'abc123'                   '%[a-z]'     -> array (  0 => 'abc',)
'abc123'                   '%[^0-9]'    -> array (  0 => 'abc',)
'abc123'                   '%[]a-z]'    -> array (  0 => 'abc',)
']x'                       '%[]]'       -> array (  0 => ']',)
'abc'                      '%[a-]'      -> array (  0 => 'a',)
'-abc'                     '%[a-c-]'    -> array (  0 => '-abc',)
'a-c'                      '%[a\\-c]'   -> array (  0 => 'a',)
'a^b'                      '%[a^b]'     -> array (  0 => 'a^b',)
'A'                        '%[c-a]'     -> array (  0 => NULL,)
'abcd'                     '%[^a]'      -> array (  0 => NULL,)
## the scan's own bookkeeping
'12abc'                    '%d%n%s'     -> array (  0 => 12,  1 => 2,  2 => 'abc',)
'100%'                     '%d%%'       -> array (  0 => 100,)
'a b'                      'a%nb'       -> array (  0 => 1,)
''                         '%n'         -> array (  0 => 0,)
'hello'                    '%*s%s'      -> array (  0 => NULL,)
'hello world'              '%*s %s'     -> array (  0 => 'world',)
'5'                        '%*d'        -> array ()
''                         '%*d'        -> NULL
'5 6 7'                    '%d %d %d %d' -> array (  0 => 5,  1 => 6,  2 => 7,  3 => NULL,)
'1x2'                      '%d-%d'      -> array (  0 => 1,  1 => NULL,)
'abc'                      '%d'         -> array (  0 => NULL,)
''                         '%d'         -> NULL
'a' . "\0" . 'b'           '%s'         -> array (  0 => 'a',)
'abc'                      ''           -> array ()
''                         'abc'        -> NULL
