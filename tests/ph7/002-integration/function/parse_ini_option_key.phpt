--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_ini_string() keeps a bracketed offset, and reads php's line grammar
--FILE--
<?php
set_error_handler(function ($no, $msg) {
    echo '  [warning] ', rtrim($msg), "\n";
    return true;
});

$cases = [
    // the offset NAMES the entry; only an empty one takes the next index
    'offset word'        => "a[x] = 1\na[y] = 2",
    'offset empty'       => "a[] = 1\na[] = 2",
    'offset mixed'       => "a[] = 1\na[k] = 2\na[] = 3",
    'offset numeric'     => "a[3] = 1\na[] = 2",
    'offset leading nul' => "a[03] = 1",
    'offset blanks'      => "a[ 4 ] = 1\na[ x y ] = 2",
    'offset quoted'      => "a[\"q r\"] = 1\nb['s t'] = 2",
    'offset negative'    => "a[-2] = 1",
    'offset duplicated'  => "a[x] = 1\na[x] = 2",
    // ST_OFFSET's run rule is far wider than a value's: no operators here
    'offset operators'   => "a[1|2] = 1\nb[x=y] = 2\nc[x#y] = 3\nd[x.y] = 4",
    // a run that is exactly an identifier is a CONSTANT, and nothing else is
    'offset constant'    => "a[PHP_EOL] = 1",
    'offset undefined'   => "a[NOPE_XYZ] = 1",
    'offset true'        => "a[true] = 1",
    'offset false'       => "a[false] = 1\na[null] = 2",
    'offset boolword'    => "a[on] = 1\nb[yes] = 2\nc[off] = 3",
    // a KEPT section opens its own option namespace; where the sections are
    // not kept every option shares the one array, and a reopened one merges
    'offset per section' => "[s]\na[x] = 1\n[t]\na[x] = 2",
    'offset flat section' => "a[x] = 1\n[s]\na[y] = 2",
    // an offset that never closes, and a second one, are php's syntax errors
    'offset unclosed'    => "a[x = 1",
    'offset to newline'  => "a[x\nb = 2",
    'offset semicolon'   => "a[x;y] = 1",
    'offset nested'      => "a[x][y] = 1",
    // `#` has not been a comment since the hash form was dropped
    'hash whole line'    => "# a = 1\nb = 2",
    'hash inline'        => "a = 1 # tail",
    'hash no space'      => "a = 1# tail",
    'hash alone'         => "# c\nb = 2",
    'semi inline'        => "a = 1 ; tail",
    // a label with no `=` is dropped, and never eats the line behind it
    'label alone'        => "justaword",
    'label then entry'   => "justaword\nb = 2",
    // the blanks behind a value belong to the newline that ends it -- unless
    // there is no newline, and the value runs out at the end of the file
    'blanks then newline' => "b = ends here   \n",
    'blanks then eof'     => "b = ends here   ",
    'tabs then eof'       => "b = ends\t\t",
    'quoted then eof'     => "b = \"q\"  ",
    'blanks then cr'      => "b = x  \r",
    'blanks then crlf'    => "b = x  \r\n",
];

foreach ($cases as $zLabel => $zIni) {
    echo $zLabel, "\n";
    var_export(parse_ini_string($zIni, true));
    echo "\n";
}

// with the sections NOT kept there is one array for every option, and the
// memo of the arrays an offset has opened is shared across the whole file
$flat = [
    'flat reopened'  => "a[x] = 1\n[s]\na[y] = 2",
    'flat overwrite' => "[s]\na[x] = 1\n[t]\na[x] = 2",
];
foreach ($flat as $zLabel => $zIni) {
    echo $zLabel, "\n";
    var_export(parse_ini_string($zIni, false));
    echo "\n";
}
?>
--EXPECT--
offset word
array (
  'a' => 
  array (
    'x' => '1',
    'y' => '2',
  ),
)
offset empty
array (
  'a' => 
  array (
    0 => '1',
    1 => '2',
  ),
)
offset mixed
array (
  'a' => 
  array (
    0 => '1',
    'k' => '2',
    1 => '3',
  ),
)
offset numeric
array (
  'a' => 
  array (
    3 => '1',
    4 => '2',
  ),
)
offset leading nul
array (
  'a' => 
  array (
    '03' => '1',
  ),
)
offset blanks
array (
  'a' => 
  array (
    '4 ' => '1',
    'x y ' => '2',
  ),
)
offset quoted
array (
  'a' => 
  array (
    'q r' => '1',
  ),
  'b' => 
  array (
    's t' => '2',
  ),
)
offset negative
array (
  'a' => 
  array (
    -2 => '1',
  ),
)
offset duplicated
array (
  'a' => 
  array (
    'x' => '2',
  ),
)
offset operators
array (
  'a' => 
  array (
    '1|2' => '1',
  ),
  'b' => 
  array (
    'x=y' => '2',
  ),
  'c' => 
  array (
    'x#y' => '3',
  ),
  'd' => 
  array (
    'x.y' => '4',
  ),
)
offset constant
array (
  'a' => 
  array (
    '
' => '1',
  ),
)
offset undefined
array (
  'a' => 
  array (
    'NOPE_XYZ' => '1',
  ),
)
offset true
array (
  'a' => 
  array (
    1 => '1',
  ),
)
offset false
array (
  'a' => 
  array (
    0 => '1',
    1 => '2',
  ),
)
offset boolword
array (
  'a' => 
  array (
    'on' => '1',
  ),
  'b' => 
  array (
    'yes' => '2',
  ),
  'c' => 
  array (
    'off' => '3',
  ),
)
offset per section
array (
  's' => 
  array (
    'a' => 
    array (
      'x' => '1',
    ),
  ),
  't' => 
  array (
    'a' => 
    array (
      'x' => '2',
    ),
  ),
)
offset flat section
array (
  'a' => 
  array (
    'x' => '1',
  ),
  's' => 
  array (
    'a' => 
    array (
      'y' => '2',
    ),
  ),
)
offset unclosed
  [warning] syntax error, unexpected end of file, expecting ']' in Unknown on line 1
false
offset to newline
  [warning] syntax error, unexpected end of file, expecting ']' in Unknown on line 1
false
offset semicolon
  [warning] syntax error, unexpected end of file, expecting ']' in Unknown on line 1
false
offset nested
  [warning] syntax error, unexpected TC_SECTION, expecting '=' in Unknown on line 1
false
hash whole line
array (
  '# a' => '1',
  'b' => '2',
)
hash inline
array (
  'a' => '1 # tail',
)
hash no space
array (
  'a' => '1# tail',
)
hash alone
array (
  'b' => '2',
)
semi inline
array (
  'a' => '1',
)
label alone
array (
)
label then entry
array (
  'b' => '2',
)
blanks then newline
array (
  'b' => 'ends here',
)
blanks then eof
array (
  'b' => 'ends here   ',
)
tabs then eof
array (
  'b' => 'ends		',
)
quoted then eof
array (
  'b' => 'q',
)
blanks then cr
array (
  'b' => 'x',
)
blanks then crlf
array (
  'b' => 'x',
)
flat reopened
array (
  'a' => 
  array (
    'x' => '1',
    'y' => '2',
  ),
)
flat overwrite
array (
  'a' => 
  array (
    'x' => '2',
  ),
)
