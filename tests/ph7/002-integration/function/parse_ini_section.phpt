--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_ini_string() reads a section name with the value grammar, and ${NAME:-fallback}
--FILE--
<?php
define('MYC', 'cval');
putenv('SETENV=envval');
putenv('EMPTYENV=');

set_error_handler(function ($no, $msg) {
    echo '  [warning] ', rtrim($msg), "\n";
    return true;
});

function show(string $label, string $ini, bool $sections = true, int $mode = INI_SCANNER_NORMAL): void
{
    echo $label, "\n";
    $r = parse_ini_string($ini, $sections, $mode);
    echo '  ', str_replace("\n", '', var_export($r, true)), "\n";
}

echo "== the section name runs the value grammar\n";
// blanks are content: nothing is trimmed off either end
show('[ a ]', "[ a ]\nk=1\n");
// a ${...} is expanded, and its pieces concatenate with the rest
show('[${SETENV}x]', "[\${SETENV}x]\nk=1\n");
// but a bare identifier is NOT a constant here: php's section production is
// constant_literal, which keeps the name as written
show('[MYC]', "[MYC]\nk=1\n");
// quoted pieces: double quotes collapse the three escapes, single quotes none
show('["a\$b"]', "[\"a\\\$b\"]\nk=1\n");
show("['r a w']", "['r a w']\nk=1\n");
// a backslash takes the byte behind it, so this `]` does not end the section
show('[a\]b]', "[a\\]b]\nk=1\n");
// a `$` that opens nothing takes the byte behind it too -- and here that byte
// is the closing bracket, so the section never ends
show('[$]', "[\$]\nk=1\n");
// neither a `;` nor a newline has any rule inside a section
show('[a;b]', "[a;b]\nk=1\n");
show('[unclosed', "[unclosed\nk=1\n");
// an EMPTY name is a section like any other, and every option behind it lands
// there instead of at the top level
show('[]', "[a]\nk=1\n[]\nj=2\n");

echo "== INI_SCANNER_RAW interprets none of it\n";
show('[ a ] raw', "[ a ]\nk=1\n", true, INI_SCANNER_RAW);
show('[${SETENV}] raw', "[\${SETENV}]\nk=1\n", true, INI_SCANNER_RAW);
show('["q"] raw', "[\"q\"]\nk=1\n", true, INI_SCANNER_RAW);
show('[a;b] raw', "[a;b]\nk=1\n", true, INI_SCANNER_RAW);

echo "== \${NAME:-fallback}\n";
// the fallback is reached only when the name is UNSET: a name set to the empty
// string answers that empty string
show('${SETENV:-fb}', "a=\${SETENV:-fb}\n", false);
// Windows keeps two environments: getenv() asks Win32, which holds the empty
// value, while php's ini lookup reads the C runtime's, where putenv('X=')
// REMOVED it -- so php there answers the fallback. That cell is the platform's,
// not the parser's, and is left unpinned on Windows.
$r = parse_ini_string("a=\${EMPTYENV:-fb}\n", false);
echo '${EMPTYENV:-fb} is the variable, not the fallback: ',
    var_export(PHP_OS_FAMILY === 'Windows' ? in_array($r['a'], ['', 'fb'], true)
        : $r['a'] === (getenv('EMPTYENV') === '' ? '' : 'fb'), true), "\n";
show('${NOPE:-fb}', "a=\${NOPE:-fb}\n", false);
show('${NOPE:-}', "a=\${NOPE:-}\n", false);
// the fallback is a var_string_list of its own: constants, quoted pieces and
// nested variables all work, and blanks are kept
show('${NOPE:-MYC}', "a=\${NOPE:-MYC}\n", false);
show('${NOPE:-a b}', "a=\${NOPE:-a b}\n", false);
show('${NOPE:-"q"}', "a=\${NOPE:-\"q\"}\n", false);
show('${NOPE:-${NOPE2:-deep}}', "a=\${NOPE:-\${NOPE2:-deep}}\n", false);
// `&`, `|` and `(` are ordinary bytes inside a fallback, never operators
show('${NOPE:-1|2}', "a=\${NOPE:-1|2}\n", false);
// a `'` and a `;` have no rule at all in there
show("\${NOPE:-'r'}", "a=\${NOPE:-'r'}\n", false);
// every position takes one: a value, an offset and a section name
show('a=x${NOPE:-y}z', "a=x\${NOPE:-y}z\n", false);
show('a[${NOPE:-k}]', "a[\${NOPE:-k}]=1\n", false);
show('[${NOPE:-s}]', "[\${NOPE:-s}]\nk=1\n");

echo "== the malformed variable\n";
show('${}', "a=\${}\n", false);
show('${:-x}', "a=\${:-x}\n", false);
show('${NOPE]', "[\${NOPE]\nk=1\n");
show('${${SETENV}}', "[\${\${SETENV}}]\nk=1\n");

echo "== a statement can never start with `=`\n";
// and php's section rule counts a line whether or not it ate a newline, so a
// statement behind the bracket is reported one line further down
show('[] = 1', "[] = 1\n");
show('= 1', "= 1\n");
show('[a] then = 1', "[a]\n= 1\n");
?>
--EXPECT--
== the section name runs the value grammar
[ a ]
  array (  ' a ' =>   array (    'k' => '1',  ),)
[${SETENV}x]
  array (  'envvalx' =>   array (    'k' => '1',  ),)
[MYC]
  array (  'MYC' =>   array (    'k' => '1',  ),)
["a\$b"]
  array (  'a$b' =>   array (    'k' => '1',  ),)
['r a w']
  array (  'r a w' =>   array (    'k' => '1',  ),)
[a\]b]
  array (  'a\\]b' =>   array (    'k' => '1',  ),)
[$]
  [warning] syntax error, unexpected end of file, expecting ']' in Unknown on line 1
  false
[a;b]
  [warning] syntax error, unexpected end of file, expecting ']' in Unknown on line 1
  false
[unclosed
  [warning] syntax error, unexpected end of file, expecting ']' in Unknown on line 1
  false
[]
  array (  'a' =>   array (    'k' => '1',  ),  '' =>   array (    'j' => '2',  ),)
== INI_SCANNER_RAW interprets none of it
[ a ] raw
  array (  ' a ' =>   array (    'k' => '1',  ),)
[${SETENV}] raw
  array (  '${SETENV}' =>   array (    'k' => '1',  ),)
["q"] raw
  array (  '"q"' =>   array (    'k' => '1',  ),)
[a;b] raw
  array (  'a;b' =>   array (    'k' => '1',  ),)
== ${NAME:-fallback}
${SETENV:-fb}
  array (  'a' => 'envval',)
${EMPTYENV:-fb} is the variable, not the fallback: true
${NOPE:-fb}
  array (  'a' => 'fb',)
${NOPE:-}
  array (  'a' => '',)
${NOPE:-MYC}
  array (  'a' => 'cval',)
${NOPE:-a b}
  array (  'a' => 'a b',)
${NOPE:-"q"}
  array (  'a' => 'q',)
${NOPE:-${NOPE2:-deep}}
  array (  'a' => 'deep',)
${NOPE:-1|2}
  array (  'a' => '1|2',)
${NOPE:-'r'}
  [warning] syntax error, unexpected end of file, expecting '}' in Unknown on line 1
  false
a=x${NOPE:-y}z
  array (  'a' => 'xyz',)
a[${NOPE:-k}]
  array (  'a' =>   array (    'k' => '1',  ),)
[${NOPE:-s}]
  array (  's' =>   array (    'k' => '1',  ),)
== the malformed variable
${}
  [warning] syntax error, unexpected '}', expecting TC_VARNAME in Unknown on line 1
  false
${:-x}
  [warning] syntax error, unexpected TC_FALLBACK, expecting TC_VARNAME in Unknown on line 1
  false
${NOPE]
  [warning] syntax error, unexpected end of file, expecting TC_FALLBACK or '}' in Unknown on line 1
  false
${${SETENV}}
  [warning] syntax error, unexpected end of file, expecting TC_VARNAME in Unknown on line 1
  false
== a statement can never start with `=`
[] = 1
  [warning] syntax error, unexpected '=' in Unknown on line 2
  false
= 1
  [warning] syntax error, unexpected '=' in Unknown on line 1
  false
[a] then = 1
  [warning] syntax error, unexpected '=' in Unknown on line 2
  false
