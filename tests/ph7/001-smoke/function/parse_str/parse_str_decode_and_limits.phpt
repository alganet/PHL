--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_str decodes the whole name before it looks for brackets, and obeys both input limits
--FILE--
<?php
// php url-decodes the WHOLE key first and only then parses it for brackets, so
// "a%5Bb%5D" is the nested a[b] and not a flat key spelled "a[b]".
$cases = [
    'a%5Bb%5D=1', 'a%5B%5D=1', 'a[%5D]=1', 'a[%5B]=1', '%61=1',
    // Leading spaces in a name are SKIPPED, not mangled; interior spaces and
    // dots become '_', and only the part before the first '[' is mangled.
    ' a=1', '  a  =1', 'a b=1', 'a.b=1', 'a+b=1', 'a[b c]=1', 'a[b.c]=1', 'a[ b ]=1',
    // An unterminated '[' un-terminates the name: the bracket itself becomes '_'.
    'a[b=1', 'a[b]c=1', 'a[b][=1', 'a]b=1', '[a]=1', '=1',
    // "[]" appends -- and so does "[ ]", because php skips one space before
    // testing for the closing bracket.
    'a[]=1&a[]=2', 'a[ ]=1', 'a[][]=1', 'a[][b]=1&a[][b]=2', 'a[]=1&a[5]=2&a[]=3',
    // A scalar in the way is replaced by an array, and vice versa.
    'a=2&a[]=1', 'a[]=1&a=2', 'a[b][c]=1&a[b]=2', 'a[b]=1&a[b][c]=2',
    // Separator runs collapse; a pair with no '=' has an empty value; an
    // embedded NUL ends the input the way a C string does.
    'a=1&&b=2', '&a=1', 'a=1&', 'a=1&b', 'x=a=b', "a\0b=1",
    // Numeric-looking keys follow php's array-key rules.
    '0=x', '0[1]=x', '-1=x', '01=x', '1.5=x',
    // No key is special to parse_str itself.
    'GLOBALS=1', 'GLOBALS[a]=1', 'this=1',
];
foreach ($cases as $c) {
    $r = null;
    parse_str($c, $r);
    echo str_replace(["\0", "\n"], ['\0', ''], $c), ' => ', str_replace("\n", '', var_export($r, true)), "\n";
}

// Both limits are php.ini directives, and both warn in php's own words.
set_error_handler(function ($no, $msg) { echo "W: $msg\n"; return true; });
$many = [];
for ($i = 0; $i < 1002; $i++) { $many[] = "k$i=$i"; }
$r = null;
parse_str(implode('&', $many), $r);
echo 'vars=', count($r), "\n";
$r = null;
parse_str('a' . str_repeat('[x]', 80) . '=1', $r);
echo 'deep=', count($r), "\n";
$r = null;
parse_str('b' . str_repeat('[x]', 63) . '=1', $r);
echo 'ok63=', count($r), "\n";
restore_error_handler();
echo 'ini=', ini_get('max_input_vars'), '|', ini_get('max_input_nesting_level'),
    '|', ini_get('arg_separator.input'), "\n";

// $result is by reference and is REPLACED, empty result included.
$r = ['stale' => 1];
parse_str('', $r);
var_dump($r);
try {
    parse_str('a=1');
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
try {
    parse_str('a=1', $r, 3);
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
try {
    parse_str([1], $r);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
--EXPECT--
a%5Bb%5D=1 => array (  'a' =>   array (    'b' => '1',  ),)
a%5B%5D=1 => array (  'a' =>   array (    0 => '1',  ),)
a[%5D]=1 => array (  'a' =>   array (    0 => '1',  ),)
a[%5B]=1 => array (  'a' =>   array (    '[' => '1',  ),)
%61=1 => array (  'a' => '1',)
 a=1 => array (  'a' => '1',)
  a  =1 => array (  'a__' => '1',)
a b=1 => array (  'a_b' => '1',)
a.b=1 => array (  'a_b' => '1',)
a+b=1 => array (  'a_b' => '1',)
a[b c]=1 => array (  'a' =>   array (    'b c' => '1',  ),)
a[b.c]=1 => array (  'a' =>   array (    'b.c' => '1',  ),)
a[ b ]=1 => array (  'a' =>   array (    ' b ' => '1',  ),)
a[b=1 => array (  'a_b' => '1',)
a[b]c=1 => array (  'a' =>   array (    'b' => '1',  ),)
a[b][=1 => array (  'a' =>   array (    'b' => '1',  ),)
a]b=1 => array (  'a]b' => '1',)
[a]=1 => array ()
=1 => array ()
a[]=1&a[]=2 => array (  'a' =>   array (    0 => '1',    1 => '2',  ),)
a[ ]=1 => array (  'a' =>   array (    0 => '1',  ),)
a[][]=1 => array (  'a' =>   array (    0 =>     array (      0 => '1',    ),  ),)
a[][b]=1&a[][b]=2 => array (  'a' =>   array (    0 =>     array (      'b' => '1',    ),    1 =>     array (      'b' => '2',    ),  ),)
a[]=1&a[5]=2&a[]=3 => array (  'a' =>   array (    0 => '1',    5 => '2',    6 => '3',  ),)
a=2&a[]=1 => array (  'a' =>   array (    0 => '1',  ),)
a[]=1&a=2 => array (  'a' => '2',)
a[b][c]=1&a[b]=2 => array (  'a' =>   array (    'b' => '2',  ),)
a[b]=1&a[b][c]=2 => array (  'a' =>   array (    'b' =>     array (      'c' => '2',    ),  ),)
a=1&&b=2 => array (  'a' => '1',  'b' => '2',)
&a=1 => array (  'a' => '1',)
a=1& => array (  'a' => '1',)
a=1&b => array (  'a' => '1',  'b' => '',)
x=a=b => array (  'x' => 'a=b',)
a\0b=1 => array (  'a' => '',)
0=x => array (  0 => 'x',)
0[1]=x => array (  0 =>   array (    1 => 'x',  ),)
-1=x => array (  -1 => 'x',)
01=x => array (  '01' => 'x',)
1.5=x => array (  '1_5' => 'x',)
GLOBALS=1 => array (  'GLOBALS' => '1',)
GLOBALS[a]=1 => array (  'GLOBALS' =>   array (    'a' => '1',  ),)
this=1 => array (  'this' => '1',)
W: parse_str(): Input variables exceeded 1000. To increase the limit change max_input_vars in php.ini.
vars=1000
W: parse_str(): Input variable nesting level exceeded 64. To increase the limit change max_input_nesting_level in php.ini.
deep=0
ok63=1
ini=1000|64|&
array(0) {
}
parse_str() expects exactly 2 arguments, 1 given
parse_str() expects exactly 2 arguments, 3 given
parse_str(): Argument #1 ($string) must be of type string, array given
--CLEAN--
<?php
