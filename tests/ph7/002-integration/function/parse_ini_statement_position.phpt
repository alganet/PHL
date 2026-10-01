--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_ini_string() refuses a line that is not one of php's statements
--FILE--
<?php
set_error_handler(function ($no, $msg) {
    echo '  [warning] ', rtrim($msg), "\n";
    return true;
});

$cases = [
    // LABEL_CHAR stops at these twelve, and INITIAL hands each one to the
    // parser as itself: the line is refused, never folded into the key
    'stop and'        => "a&b = 1",
    'stop or'         => "a|b = 1",
    'stop xor'        => "a^b = 1",
    'stop dollar'     => "a\$b = 1",
    'stop tilde'      => "a~b = 1",
    'stop parens'     => "a(b) = 1",
    'stop braces'     => "a{b} = 1",
    'stop bang'       => "a!b = 1",
    'stop quote'      => "a\"b\" = 1",
    'stop bracket'    => "a]b = 1",
    'stop alone'      => "&\nok = 1",
    'stop indented'   => "  &\nok = 1",
    'stop after word' => "b &\nok = 1",
    // ...and these stay ordinary key bytes, because the run never stops there
    'kept punctuation' => "a.b = 1\nc:d = 2\ne,f = 3\ng'h = 4\ni+j = 5\nk%l = 6",
    // the byte is named where it stands, and the line it stands on is counted
    'stop on line 3'  => "a = 1\nb = 2\nc} = 3\nok = 4",
    'stop in section' => "[s]\nc} = 1\nok = 2",
    // a RAW section ends at the first `]`, and what is left of the line is a
    // statement of its own -- and not a statement php has
    'raw section tail' => "[a\\]b]\nx = 1",
    // a TAB ends the label; a bare label is a statement that does nothing, so
    // the run behind the TAB is a second statement on the same line
    'tab splits'      => "b\tc = 1",
    'tab before eq'   => "b\t= 1",
    'tab then blanks' => "b\t\t= 1",
    'semicolon ends'  => "b;x = 1\nok = 2",
    // an offset has exactly one statement, `a[x] = v`: nothing else may follow
    'offset bare'     => "a[x]\nok = 1",
    'offset bracket'  => "a[x]] = 1",
    'offset word'     => "a[x]b = 1",
    'offset token'    => "a[x]& = 1",
    'offset section'  => "a[x][y] = 1",
    'offset space'    => "a[x] ] = 1",
    'offset comment'  => "a[x];c\nok = 1",
    'offset eof'      => "a[x]",
    // ...while the blanks php's own rule eats still reach the `=`
    'offset spaced'   => "a[x] = 1",
    'offset tabbed'   => "a[x]\t= 1",
];

foreach ($cases as $name => $src) {
    echo $name, ":\n";
    var_export(parse_ini_string($src, true, INI_SCANNER_NORMAL));
    echo "\n";
}

// INI_SCANNER_RAW reads a section with a narrower rule, so the backslash is
// not an escape there and the leftover `b]` is what the parser refuses
echo "raw section tail (RAW):\n";
var_export(parse_ini_string("[a\\]b]\nx = 1", true, INI_SCANNER_RAW));
echo "\n";
--EXPECT--
stop and:
  [warning] syntax error, unexpected '&' in Unknown on line 1
false
stop or:
  [warning] syntax error, unexpected '|' in Unknown on line 1
false
stop xor:
  [warning] syntax error, unexpected '^' in Unknown on line 1
false
stop dollar:
  [warning] syntax error, unexpected '$' in Unknown on line 1
false
stop tilde:
  [warning] syntax error, unexpected '~' in Unknown on line 1
false
stop parens:
  [warning] syntax error, unexpected '(' in Unknown on line 1
false
stop braces:
  [warning] syntax error, unexpected '{' in Unknown on line 1
false
stop bang:
  [warning] syntax error, unexpected '!' in Unknown on line 1
false
stop quote:
  [warning] syntax error, unexpected '"' in Unknown on line 1
false
stop bracket:
  [warning] syntax error, unexpected ']' in Unknown on line 1
false
stop alone:
  [warning] syntax error, unexpected '&' in Unknown on line 1
false
stop indented:
  [warning] syntax error, unexpected '&' in Unknown on line 1
false
stop after word:
  [warning] syntax error, unexpected '&' in Unknown on line 1
false
kept punctuation:
array (
  'a.b' => '1',
  'c:d' => '2',
  'e,f' => '3',
  'g\'h' => '4',
  'i+j' => '5',
  'k%l' => '6',
)
stop on line 3:
  [warning] syntax error, unexpected '}' in Unknown on line 3
false
stop in section:
  [warning] syntax error, unexpected '}' in Unknown on line 2
false
raw section tail:
array (
  'a\\]b' => 
  array (
    'x' => '1',
  ),
)
tab splits:
array (
  'c' => '1',
)
tab before eq:
array (
  'b' => '1',
)
tab then blanks:
array (
  'b' => '1',
)
semicolon ends:
array (
  'ok' => '2',
)
offset bare:
  [warning] syntax error, unexpected END_OF_LINE, expecting '=' in Unknown on line 2
false
offset bracket:
  [warning] syntax error, unexpected ']', expecting '=' in Unknown on line 1
false
offset word:
  [warning] syntax error, unexpected TC_LABEL, expecting '=' in Unknown on line 1
false
offset token:
  [warning] syntax error, unexpected '&', expecting '=' in Unknown on line 1
false
offset section:
  [warning] syntax error, unexpected TC_SECTION, expecting '=' in Unknown on line 1
false
offset space:
  [warning] syntax error, unexpected TC_LABEL, expecting '=' in Unknown on line 1
false
offset comment:
  [warning] syntax error, unexpected END_OF_LINE, expecting '=' in Unknown on line 2
false
offset eof:
  [warning] syntax error, unexpected end of file, expecting '=' in Unknown on line 1
false
offset spaced:
array (
  'a' => 
  array (
    'x' => '1',
  ),
)
offset tabbed:
array (
  'a' => 
  array (
    'x' => '1',
  ),
)
raw section tail (RAW):
  [warning] syntax error, unexpected ']' in Unknown on line 2
false
