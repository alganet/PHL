--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_ini_string() evaluates the ini bitwise-expression grammar
--FILE--
<?php
define('MYC', 'hello');

set_error_handler(function ($no, $msg) {
    // the ini file names itself in the warning: keep only its basename
    // php ends the message with a newline and names the file it read
    $msg = preg_replace('#in \S*parse_ini_expr\S*\.ini#', 'in <ini file>', rtrim($msg));
    echo '  [warning] ', $msg, "\n";
    return true;
});

$cases = [
    // no operator anywhere: the pieces concatenate
    'a = foo bar',
    'a = MYC and more',
    'a = MYC.x',
    'a = x "y" z',
    'a = 3+4',
    'a = 1 2 3',
    'a = C:\path\to',
    // an operator makes it a 32-bit bitwise expression over atoi()
    'a = E_ALL & ~E_NOTICE',
    'a = E_ALL & ~ E_DEPRECATED',
    'a = 1|2',
    'a = 1 | 2',
    'a = 1|2|4',
    'a = 1&2^3|4',
    'a = (1|2)&3',
    'a = ( 1 | 2 )',
    'a = 5^3',
    'a = ~0',
    'a = ~~2',
    'a = !0',
    'a = !!3',
    'a = 8&~8',
    'a = foo|bar',
    'a = hello world & stuff',
    'a = "a"|"b"',
    'a = 1|"2"',
    'a = 1.5|0',
    'a = 0x10|0',
    'a = -1|0',
    // atoi() reads a long and answers an int (past INT_MAX, the platform's
    // long decides: parse_ini_expression_long_width)
    'a = 2147483647|0',
    // a quoted run is bytes, and holds no operator
    'a = "1|2"',
    "a = 'sq|pipe'",
    'a = "x;y"',
    // the boolean words are whole-value tokens
    'a = On',
    'a = none',
    'a = null',
    'a = ONx',
    'a = "on"',
    // syntax errors: one bad value discards the whole parse
    'a = &',
    'a = 1 &',
    'a = |x',
    'a = (',
    'a = (1',
    'a = 1)',
    'a = hello!',
    'a = foo!bar',
    'a = something (note)',
    'a = 1 ~ 2',
    'a = TRUE|FALSE',
    'a = on x',
    'a = x on',
    'a = 3 no',
    'a = x null',
    'a = (on)',
    "a = 1\nb = &\nc = 3",
];
foreach ($cases as $ini) {
    echo str_replace("\n", '\n', $ini), "\n";
    $r = parse_ini_string($ini, false, INI_SCANNER_NORMAL);
    echo '  NORMAL ', $r === false ? 'PARSE-FAILED' : var_export($r['a'], true), "\n";
    $r = parse_ini_string($ini, false, INI_SCANNER_TYPED);
    echo '  TYPED  ', $r === false ? 'PARSE-FAILED' : var_export($r['a'], true), "\n";
}

echo "--- a file names itself in the warning\n";
$path = sys_get_temp_dir() . '/parse_ini_expr_' . getmypid() . '.ini';
file_put_contents($path, "x = E_ALL & ~E_NOTICE\ny = 1\nz = 2 (3)\n");
$r = parse_ini_file($path);
echo 'file ', $r === false ? 'PARSE-FAILED' : var_export($r, true), "\n";
unlink($path);
?>
--EXPECT--
a = foo bar
  NORMAL 'foo bar'
  TYPED  'foo bar'
a = MYC and more
  NORMAL 'hello and more'
  TYPED  'hello and more'
a = MYC.x
  NORMAL 'MYC.x'
  TYPED  'MYC.x'
a = x "y" z
  NORMAL 'xyz'
  TYPED  'xyz'
a = 3+4
  NORMAL '3+4'
  TYPED  '3+4'
a = 1 2 3
  NORMAL '1 2 3'
  TYPED  '1 2 3'
a = C:\path\to
  NORMAL 'C:\\path\\to'
  TYPED  'C:\\path\\to'
a = E_ALL & ~E_NOTICE
  NORMAL '30711'
  TYPED  30711
a = E_ALL & ~ E_DEPRECATED
  NORMAL '22527'
  TYPED  22527
a = 1|2
  NORMAL '3'
  TYPED  3
a = 1 | 2
  NORMAL '3'
  TYPED  3
a = 1|2|4
  NORMAL '7'
  TYPED  7
a = 1&2^3|4
  NORMAL '7'
  TYPED  7
a = (1|2)&3
  NORMAL '3'
  TYPED  3
a = ( 1 | 2 )
  NORMAL '3'
  TYPED  3
a = 5^3
  NORMAL '6'
  TYPED  6
a = ~0
  NORMAL '-1'
  TYPED  -1
a = ~~2
  NORMAL '2'
  TYPED  2
a = !0
  NORMAL '1'
  TYPED  1
a = !!3
  NORMAL '1'
  TYPED  1
a = 8&~8
  NORMAL '0'
  TYPED  0
a = foo|bar
  NORMAL '0'
  TYPED  0
a = hello world & stuff
  NORMAL '0'
  TYPED  0
a = "a"|"b"
  NORMAL '0'
  TYPED  0
a = 1|"2"
  NORMAL '3'
  TYPED  3
a = 1.5|0
  NORMAL '1'
  TYPED  1
a = 0x10|0
  NORMAL '0'
  TYPED  0
a = -1|0
  NORMAL '-1'
  TYPED  -1
a = 2147483647|0
  NORMAL '2147483647'
  TYPED  2147483647
a = "1|2"
  NORMAL '1|2'
  TYPED  '1|2'
a = 'sq|pipe'
  NORMAL 'sq|pipe'
  TYPED  'sq|pipe'
a = "x;y"
  NORMAL 'x;y'
  TYPED  'x;y'
a = On
  NORMAL '1'
  TYPED  true
a = none
  NORMAL ''
  TYPED  false
a = null
  NORMAL ''
  TYPED  NULL
a = ONx
  NORMAL 'ONx'
  TYPED  'ONx'
a = "on"
  NORMAL 'on'
  TYPED  'on'
a = &
  [warning] syntax error, unexpected '&' in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected '&' in Unknown on line 1
  TYPED  PARSE-FAILED
a = 1 &
  [warning] syntax error, unexpected END_OF_LINE in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected END_OF_LINE in Unknown on line 1
  TYPED  PARSE-FAILED
a = |x
  [warning] syntax error, unexpected '|' in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected '|' in Unknown on line 1
  TYPED  PARSE-FAILED
a = (
  [warning] syntax error, unexpected END_OF_LINE in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected END_OF_LINE in Unknown on line 1
  TYPED  PARSE-FAILED
a = (1
  [warning] syntax error, unexpected END_OF_LINE, expecting '^' or '|' or '&' or ')' in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected END_OF_LINE, expecting '^' or '|' or '&' or ')' in Unknown on line 1
  TYPED  PARSE-FAILED
a = 1)
  [warning] syntax error, unexpected ')' in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected ')' in Unknown on line 1
  TYPED  PARSE-FAILED
a = hello!
  [warning] syntax error, unexpected '!' in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected '!' in Unknown on line 1
  TYPED  PARSE-FAILED
a = foo!bar
  [warning] syntax error, unexpected '!' in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected '!' in Unknown on line 1
  TYPED  PARSE-FAILED
a = something (note)
  [warning] syntax error, unexpected '(' in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected '(' in Unknown on line 1
  TYPED  PARSE-FAILED
a = 1 ~ 2
  [warning] syntax error, unexpected '~' in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected '~' in Unknown on line 1
  TYPED  PARSE-FAILED
a = TRUE|FALSE
  [warning] syntax error, unexpected '|' in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected '|' in Unknown on line 1
  TYPED  PARSE-FAILED
a = on x
  [warning] syntax error, unexpected TC_CONSTANT in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected TC_CONSTANT in Unknown on line 1
  TYPED  PARSE-FAILED
a = x on
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
  TYPED  PARSE-FAILED
a = 3 no
  [warning] syntax error, unexpected BOOL_FALSE in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected BOOL_FALSE in Unknown on line 1
  TYPED  PARSE-FAILED
a = x null
  [warning] syntax error, unexpected NULL_NULL in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected NULL_NULL in Unknown on line 1
  TYPED  PARSE-FAILED
a = (on)
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected BOOL_TRUE in Unknown on line 1
  TYPED  PARSE-FAILED
a = 1\nb = &\nc = 3
  [warning] syntax error, unexpected '&' in Unknown on line 2
  NORMAL PARSE-FAILED
  [warning] syntax error, unexpected '&' in Unknown on line 2
  TYPED  PARSE-FAILED
--- a file names itself in the warning
  [warning] syntax error, unexpected '(' in <ini file> on line 3
file PARSE-FAILED
