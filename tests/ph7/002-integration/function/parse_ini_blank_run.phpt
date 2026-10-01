--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_ini_string() reads a bool word as a value token and a blank run as a name
--FILE--
<?php
set_error_handler(function ($no, $msg) {
    echo '  [warning] ', rtrim($msg), "\n";
    return true;
});

$cases = [
    // php's INITIAL holds a rule for each bool word ahead of the one that
    // reads a LABEL, and no statement of its grammar starts with the token
    // they return -- so the word opens no entry, it refuses the whole parse
    'bool on'          => "on = 1",
    'bool true'        => "true = 1",
    'bool yes'         => "yes = 1",
    'bool false'       => "false = 1",
    'bool off'         => "off = 1",
    'bool no'          => "no = 1",
    // the four-byte word outruns the two-byte one inside it, and both name
    // the same token
    'bool none'        => "none = 1",
    'bool null'        => "null = 1",
    // the words fold, and a bare one is a statement of its own to refuse
    'bool folded'      => "ON = 1",
    'bool alone'       => "yes",
    'bool at eof'      => "on",
    // the word's `{TABS_AND_SPACES}*` tail is what lets it outrun a LABEL,
    // which stops dead at a TAB
    'bool tabbed'      => "on\t= 1",
    'bool space tab'   => "on \t= 1",
    // a longer LABEL wins, so only a word standing alone is a value token
    'bool prefix'      => "onx = 1",
    'bool dotted'      => "on.y = 1",
    'bool two words'   => "on x = 1",
    // `{LABEL}"["` outruns every one of them: an offset is still an offset
    'bool offset'      => "on[x] = 1",
    'bool offset gap'  => "on [x] = 1",
    // but a TAB breaks the bracket off the run and the word wins again
    'bool offset tab'  => "on \t[x] = 1",
    // the rule carries no leading blanks of its own, so SPACES in front of
    // the word join the LABEL and are trimmed back off it
    'bool led spaces'  => "  on = 1",
    // a TAB is no LABEL_CHAR, so the blank run is thrown away whole and the
    // word opens the next token after all
    'bool led tab'     => "\ton = 1",
    'bool led mixed'   => "  \tyes = 1",
    // the word is a value token wherever a statement may start
    'bool past section' => "[s]on = 1",
    'bool past offset'  => "a[x]on = 1",
    'bool past off tab' => "a[x]\ton = 1",
    // a SPACE is a LABEL_CHAR, so a spaces-only run in front of a bracket is
    // the offset's own name and the section is never reached
    'space section'    => "  [s]\nk = 1",
    'tab section'      => "\t[s]\nk = 1",
    'mixed section'    => "  \t[s]\nk = 1",
    'tab space section' => "\t  [s]\nk = 1",
    // and that name, trimmed away to nothing, is an entry under "" like any
    // other -- one array, reused across lines, until a section replaces it
    'empty option'     => " [x] = 1\n [y] = 2",
    'empty per section' => " [x] = 1\n[s]\n [y] = 2",
    'empty auto index' => " [] = 1\n [] = 2",
    // `{TABS_AND_SPACES}*[=]` outruns every other reading of the blanks it
    // eats, mixed TABS and SPACES alike
    'offset space tab' => "a[x] \t= 1",
    'offset tab space' => "a[x]\t = 1",
    'label space tab'  => "k \t= 1",
    // the bracket rule names the token behind an offset too
    'offset offset'    => "a[x] [y] = 1",
    'offset section'   => "a[x]\t[y] = 1",
    // `\v` and `\f` are LABEL_CHARs of php's and no blank at all: they are
    // never eaten, and EAT_LEADING_WHITESPACE leaves them on the name
    'vtab name'        => "\va = 1",
    'vtab alone'       => "\v = 1",
    'ff name'          => "\fa = 1",
    'vtab bool'        => "\von = 1",
    'vtab offset'      => "\v[x] = 1\n\v[y] = 2",
    'vtab bare'        => "\vb",
];

foreach ($cases as $name => $ini) {
    echo $name, ":\n";
    var_export(parse_ini_string($ini, true, INI_SCANNER_NORMAL));
    echo "\n";
}

// every INITIAL rule above is shared with the raw scanner
echo "bool raw:\n";
var_export(parse_ini_string("on = 1", true, INI_SCANNER_RAW));
echo "\n";
?>
--EXPECT--
bool on:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool true:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool yes:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool false:
  [warning] syntax error, unexpected BOOL_FALSE in Unknown on line 1
false
bool off:
  [warning] syntax error, unexpected BOOL_FALSE in Unknown on line 1
false
bool no:
  [warning] syntax error, unexpected BOOL_FALSE in Unknown on line 1
false
bool none:
  [warning] syntax error, unexpected BOOL_FALSE in Unknown on line 1
false
bool null:
  [warning] syntax error, unexpected NULL_NULL in Unknown on line 1
false
bool folded:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool alone:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool at eof:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool tabbed:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool space tab:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool prefix:
array (
  'onx' => '1',
)
bool dotted:
array (
  'on.y' => '1',
)
bool two words:
array (
  'on x' => '1',
)
bool offset:
array (
  'on' => 
  array (
    'x' => '1',
  ),
)
bool offset gap:
array (
  'on' => 
  array (
    'x' => '1',
  ),
)
bool offset tab:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool led spaces:
array (
  'on' => '1',
)
bool led tab:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool led mixed:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
bool past section:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 2
false
bool past offset:
  [warning] syntax error, unexpected BOOL_TRUE, expecting '=' in Unknown on line 1
false
bool past off tab:
  [warning] syntax error, unexpected BOOL_TRUE, expecting '=' in Unknown on line 1
false
space section:
  [warning] syntax error, unexpected END_OF_LINE, expecting '=' in Unknown on line 2
false
tab section:
array (
  's' => 
  array (
    'k' => '1',
  ),
)
mixed section:
array (
  's' => 
  array (
    'k' => '1',
  ),
)
tab space section:
array (
  's' => 
  array (
    'k' => '1',
  ),
)
empty option:
array (
  '' => 
  array (
    'x' => '1',
    'y' => '2',
  ),
)
empty per section:
array (
  '' => 
  array (
    'x' => '1',
  ),
  's' => 
  array (
    '' => 
    array (
      'y' => '2',
    ),
  ),
)
empty auto index:
array (
  '' => 
  array (
    0 => '1',
    1 => '2',
  ),
)
offset space tab:
array (
  'a' => 
  array (
    'x' => '1',
  ),
)
offset tab space:
array (
  'a' => 
  array (
    'x' => '1',
  ),
)
label space tab:
array (
  'k' => '1',
)
offset offset:
  [warning] syntax error, unexpected TC_OFFSET, expecting '=' in Unknown on line 1
false
offset section:
  [warning] syntax error, unexpected TC_SECTION, expecting '=' in Unknown on line 1
false
vtab name:
array (
  'a' => '1',
)
vtab alone:
array (
  '' => '1',
)
ff name:
array (
  'a' => '1',
)
vtab bool:
array (
  'on' => '1',
)
vtab offset:
array (
  '' => 
  array (
    'x' => '1',
    'y' => '2',
  ),
)
vtab bare:
array (
)
bool raw:
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
false
