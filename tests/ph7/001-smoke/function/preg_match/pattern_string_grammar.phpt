--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Read the pattern string the way php reads it, brackets and modifiers alike
--DESCRIPTION--
Every preg_* function is handed ONE string that php takes apart before PCRE2 ever
sees it: leading whitespace, a delimiter byte, the body up to its close, then the
modifier letters. Two of those rules were wrong here, and both went WRONG QUIETLY.

A bracket delimiter scans for its MATCHING close, counting nesting, so `{^a{2}$}`
is the whole `^a{2}$`; this engine stopped at the first `}` and handed PCRE2
`^a{2`, which compiles perfectly well as the literal text `a{2` and then simply
never matches -- `preg_match('{^a{2}$}', 'aa')` answered 0 with no diagnostic at
all. `((\d+))` and `[^a[bc]+$]` failed the louder way, as compile errors. The
count is brackets and nothing else: a close inside a character class or a
quantifier still counts, an escaped one does not, and a CLOSING bracket opens a
pattern of its own that closes on itself without any nesting.

The modifier letters are SCREENED, and the first byte php does not know refuses
the pattern before the body is compiled -- so `)(\d+))`, whose body is `(\d+`, is
"Unknown modifier ')'" rather than a compile failure. Unknown letters were
ignored here instead, which made `/^a$/e` and `$^abc$$` quietly match. `n` (php
8.2's no-auto-capture) was among the letters that did nothing. Beside them the
leading-whitespace rule: php skips isspace(), not every byte <= 0x20, so
"\x01^a$\x01" is a pattern delimited by \x01 and a NUL is a refused delimiter
rather than a skipped one.
--FILE--
<?php
/* One printable rendering for a pattern that may hold control bytes. */
function pcdShow($s) {
    $o = '';
    for ($i = 0; $i < strlen($s); $i++) {
        $c = $s[$i];
        $o .= ($c >= ' ' && $c <= '~') ? $c : sprintf('\x%02x', ord($c));
    }
    return $o;
}
function pcdRun($pat, $subj) {
    $m = [];
    $r = preg_match($pat, $subj, $m);
    echo '  ', str_pad(pcdShow($pat), 22), ' vs ', str_pad(pcdShow($subj), 8),
         ' -> ', var_export($r, true), ' ', json_encode($m), "\n";
}
set_error_handler(function ($no, $str) { echo '  W(', $no, '): ', pcdShow($str), "\n"; return true; });

echo "-- a bracket delimiter scans for its MATCHING close, counting nesting\n";
pcdRun('((\d+))', 'a123b');
pcdRun('(a(\d+)b)', 'a123b');
pcdRun('{^a{2}$}', 'aa');
pcdRun('{^a-{1,2}b$}', 'a--b');
pcdRun('{^(?:a{1,2}){1,2}$}', 'aa');
pcdRun('[^a[bc]+$]', 'abc');
pcdRun('<^a(?<n>b)$>', 'ab');
pcdRun('((((a))))', 'a');

echo "-- the count is brackets and nothing else: a class or a quantifier is not read\n";
pcdRun('{[{}]}', '{');
pcdRun('[a[b]c]', 'abc');
pcdRun('{a{2}}', 'aa');
pcdRun('{a\{b{c}}', 'a{bc');

echo "-- an escaped bracket changes no level, and an unbalanced one has no ending\n";
pcdRun('{a\{b}', 'a{b');
pcdRun('{a\}b}', 'a}b');
pcdRun('{a{b}', 'ab');
pcdRun('((((a)))', 'a');
pcdRun('(a(b)', 'ab');
pcdRun('{abc\\', 'abc');

echo "-- a CLOSING bracket opens a pattern of its own and closes on itself\n";
pcdRun(')^abc$)', 'abc');
pcdRun(']^abc$]', 'abc');
pcdRun('}^abc$}', 'abc');
pcdRun('>^abc$>', 'abc');
pcdRun(')(\d+))', 'a1b');

echo "-- only the six isspace() bytes are skipped before the delimiter\n";
pcdRun('  {^abc$}', 'abc');
pcdRun("\t/^abc$/", 'abc');
pcdRun("\n/^abc$/", 'abc');
pcdRun("\x0b/^abc$/", 'abc');
pcdRun("\x01^abc$\x01", 'abc');
pcdRun("\x1f^abc$\x1f", 'abc');
pcdRun("\x7f^abc$\x7f", 'abc');
pcdRun("\x00abc\x00", 'abc');
pcdRun('   ', 'abc');
pcdRun('', 'abc');
pcdRun('a^abc$a', 'abc');
pcdRun('\\^abc$\\', 'abc');
pcdRun('/abc', 'abc');

echo "-- a NUL inside the body is a byte like any other\n";
pcdRun("/ab\x00c/", "ab\x00c");
pcdRun("{ab\x00c}", "ab\x00c");
pcdRun("/a\\\x00b/", "a\x00b");

echo "-- the modifiers are SCREENED, before the body is ever compiled\n";
foreach (['i','m','s','x','u','A','D','U','J','n','S','X',' ',"\n","\r",
          "\t","\x0b","\x0c",'e','g','o','p','z','Z','I','1','!','~',
          "\x00","\x7f","\x80","\xe9"] as $pcdF) {
    pcdRun('/^a$/' . $pcdF, 'a');
}
pcdRun('/^A$/imsu', 'a');
pcdRun("/^a$/i\nm", 'a');
pcdRun("/^a$/i\tm", 'a');
pcdRun('$^abc$$', 'abc');
pcdRun('/abc/i' . "\x00", 'abc');

echo "-- /n turns the numbered groups off; a named one still captures\n";
pcdRun('/(a)(?<x>b)/n', 'ab');
pcdRun('/(a)(b)/n', 'ab');
pcdRun('/(?<x>a)(b)/n', 'ab');

echo "-- every entry point reads the pattern the same way\n";
var_dump(preg_replace('{^a{2}$}', 'X', 'aa'));
var_dump(preg_split('{a{1}}', 'XaY'));
var_dump(preg_grep('{^a{2}$}', ['aa', 'b']));
var_dump(preg_match_all('{a{1}}', 'aaa', $pcdAll), $pcdAll);
var_dump(preg_replace_callback('{^a{2}$}', fn($m) => 'X', 'aa'));
var_dump(preg_replace_callback_array(['{^a{2}$}' => fn($m) => 'X'], 'aa'));
var_dump(filter_var('aa', FILTER_VALIDATE_REGEXP, ['options' => ['regexp' => '{^a{2}$}']]));
foreach (new RegexIterator(new ArrayIterator(['aa', 'b']), '{^a{2}$}') as $pcdV) {
    echo '  it: ', $pcdV, "\n";
}
try { new RegexIterator(new ArrayIterator(['a']), '/a/Q'); }
catch (Throwable $e) { echo '  ', get_class($e), ': ', $e->getMessage(), "\n"; }

echo "-- a refused pattern leaves preg_last_error() at the internal code\n";
preg_match('(a(b)', 'x');
var_dump(preg_last_error(), preg_last_error_msg());
preg_match('/a/', 'a');
var_dump(preg_last_error(), preg_last_error_msg());
restore_error_handler();
--EXPECT--
-- a bracket delimiter scans for its MATCHING close, counting nesting
  ((\d+))                vs a123b    -> 1 ["123","123"]
  (a(\d+)b)              vs a123b    -> 1 ["a123b","123"]
  {^a{2}$}               vs aa       -> 1 ["aa"]
  {^a-{1,2}b$}           vs a--b     -> 1 ["a--b"]
  {^(?:a{1,2}){1,2}$}    vs aa       -> 1 ["aa"]
  [^a[bc]+$]             vs abc      -> 1 ["abc"]
  <^a(?<n>b)$>           vs ab       -> 1 {"0":"ab","n":"b","1":"b"}
  ((((a))))              vs a        -> 1 ["a","a","a","a"]
-- the count is brackets and nothing else: a class or a quantifier is not read
  {[{}]}                 vs {        -> 1 ["{"]
  [a[b]c]                vs abc      -> 1 ["abc"]
  {a{2}}                 vs aa       -> 1 ["aa"]
  {a\{b{c}}              vs a{bc     -> 0 []
-- an escaped bracket changes no level, and an unbalanced one has no ending
  {a\{b}                 vs a{b      -> 1 ["a{b"]
  {a\}b}                 vs a}b      -> 1 ["a}b"]
  W(2): preg_match(): No ending matching delimiter '}' found
  {a{b}                  vs ab       -> false []
  W(2): preg_match(): No ending matching delimiter ')' found
  ((((a)))               vs a        -> false []
  W(2): preg_match(): No ending matching delimiter ')' found
  (a(b)                  vs ab       -> false []
  W(2): preg_match(): No ending matching delimiter '}' found
  {abc\                  vs abc      -> false []
-- a CLOSING bracket opens a pattern of its own and closes on itself
  )^abc$)                vs abc      -> 1 ["abc"]
  ]^abc$]                vs abc      -> 1 ["abc"]
  }^abc$}                vs abc      -> 1 ["abc"]
  >^abc$>                vs abc      -> 1 ["abc"]
  W(2): preg_match(): Unknown modifier ')'
  )(\d+))                vs a1b      -> false []
-- only the six isspace() bytes are skipped before the delimiter
    {^abc$}              vs abc      -> 1 ["abc"]
  \x09/^abc$/            vs abc      -> 1 ["abc"]
  \x0a/^abc$/            vs abc      -> 1 ["abc"]
  \x0b/^abc$/            vs abc      -> 1 ["abc"]
  \x01^abc$\x01          vs abc      -> 1 ["abc"]
  \x1f^abc$\x1f          vs abc      -> 1 ["abc"]
  \x7f^abc$\x7f          vs abc      -> 1 ["abc"]
  W(2): preg_match(): Delimiter must not be alphanumeric, backslash, or NUL byte
  \x00abc\x00            vs abc      -> false []
  W(2): preg_match(): Empty regular expression
                         vs abc      -> false []
  W(2): preg_match(): Empty regular expression
                         vs abc      -> false []
  W(2): preg_match(): Delimiter must not be alphanumeric, backslash, or NUL byte
  a^abc$a                vs abc      -> false []
  W(2): preg_match(): Delimiter must not be alphanumeric, backslash, or NUL byte
  \^abc$\                vs abc      -> false []
  W(2): preg_match(): No ending delimiter '/' found
  /abc                   vs abc      -> false []
-- a NUL inside the body is a byte like any other
  /ab\x00c/              vs ab\x00c  -> 1 ["ab\u0000c"]
  {ab\x00c}              vs ab\x00c  -> 1 ["ab\u0000c"]
  /a\\x00b/              vs a\x00b   -> 1 ["a\u0000b"]
-- the modifiers are SCREENED, before the body is ever compiled
  /^a$/i                 vs a        -> 1 ["a"]
  /^a$/m                 vs a        -> 1 ["a"]
  /^a$/s                 vs a        -> 1 ["a"]
  /^a$/x                 vs a        -> 1 ["a"]
  /^a$/u                 vs a        -> 1 ["a"]
  /^a$/A                 vs a        -> 1 ["a"]
  /^a$/D                 vs a        -> 1 ["a"]
  /^a$/U                 vs a        -> 1 ["a"]
  /^a$/J                 vs a        -> 1 ["a"]
  /^a$/n                 vs a        -> 1 ["a"]
  /^a$/S                 vs a        -> 1 ["a"]
  /^a$/X                 vs a        -> 1 ["a"]
  /^a$/                  vs a        -> 1 ["a"]
  /^a$/\x0a              vs a        -> 1 ["a"]
  /^a$/\x0d              vs a        -> 1 ["a"]
  W(2): preg_match(): Unknown modifier '\x09'
  /^a$/\x09              vs a        -> false []
  W(2): preg_match(): Unknown modifier '\x0b'
  /^a$/\x0b              vs a        -> false []
  W(2): preg_match(): Unknown modifier '\x0c'
  /^a$/\x0c              vs a        -> false []
  W(2): preg_match(): Unknown modifier 'e'
  /^a$/e                 vs a        -> false []
  W(2): preg_match(): Unknown modifier 'g'
  /^a$/g                 vs a        -> false []
  W(2): preg_match(): Unknown modifier 'o'
  /^a$/o                 vs a        -> false []
  W(2): preg_match(): Unknown modifier 'p'
  /^a$/p                 vs a        -> false []
  W(2): preg_match(): Unknown modifier 'z'
  /^a$/z                 vs a        -> false []
  W(2): preg_match(): Unknown modifier 'Z'
  /^a$/Z                 vs a        -> false []
  W(2): preg_match(): Unknown modifier 'I'
  /^a$/I                 vs a        -> false []
  W(2): preg_match(): Unknown modifier '1'
  /^a$/1                 vs a        -> false []
  W(2): preg_match(): Unknown modifier '!'
  /^a$/!                 vs a        -> false []
  W(2): preg_match(): Unknown modifier '~'
  /^a$/~                 vs a        -> false []
  W(2): preg_match(): NUL byte is not a valid modifier
  /^a$/\x00              vs a        -> false []
  W(2): preg_match(): Unknown modifier '\x7f'
  /^a$/\x7f              vs a        -> false []
  W(2): preg_match(): Unknown modifier '\x80'
  /^a$/\x80              vs a        -> false []
  W(2): preg_match(): Unknown modifier '\xe9'
  /^a$/\xe9              vs a        -> false []
  /^A$/imsu              vs a        -> 1 ["a"]
  /^a$/i\x0am            vs a        -> 1 ["a"]
  W(2): preg_match(): Unknown modifier '\x09'
  /^a$/i\x09m            vs a        -> false []
  W(2): preg_match(): Unknown modifier '$'
  $^abc$$                vs abc      -> false []
  W(2): preg_match(): NUL byte is not a valid modifier
  /abc/i\x00             vs abc      -> false []
-- /n turns the numbered groups off; a named one still captures
  /(a)(?<x>b)/n          vs ab       -> 1 {"0":"ab","x":"b","1":"b"}
  /(a)(b)/n              vs ab       -> 1 ["ab"]
  /(?<x>a)(b)/n          vs ab       -> 1 {"0":"ab","x":"a","1":"a"}
-- every entry point reads the pattern the same way
string(1) "X"
array(2) {
  [0]=>
  string(1) "X"
  [1]=>
  string(1) "Y"
}
array(1) {
  [0]=>
  string(2) "aa"
}
int(3)
array(1) {
  [0]=>
  array(3) {
    [0]=>
    string(1) "a"
    [1]=>
    string(1) "a"
    [2]=>
    string(1) "a"
  }
}
string(1) "X"
string(1) "X"
string(2) "aa"
  it: aa
  InvalidArgumentException: RegexIterator::__construct(): Unknown modifier 'Q'
-- a refused pattern leaves preg_last_error() at the internal code
  W(2): preg_match(): No ending matching delimiter ')' found
int(1)
string(14) "Internal error"
int(0)
string(8) "No error"
