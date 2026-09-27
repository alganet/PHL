--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A glob pattern matches across directory levels, and its bracket set means what php's does
--DESCRIPTION--
glob(3) walks EVERY segment of a pattern; this engine read only the last one, so
`src/*` . `/*.php` -- the everyday two-level spelling -- answered the empty array in
silence, as did every deeper one and every pattern ending in a slash (which names
DIRECTORIES and keeps the slash). Under both of those sits the matcher the three
pattern functions share, and four of its rules were wrong: a `*` followed by a
bracket set matched NOTHING at all (the guard that replaced SQLite's assert is
always true), a set matched nothing when escaping was turned off (the same guard,
read backwards), POSIX's `[!...]` negation was read as a literal `!` so the answer
was INVERTED, and neither the POSIX character classes nor FNM_CASEFOLD reached
inside a set (where a class meets FNM_CASEFOLD, php answers whatever the C
library does, so that one cell is left unpinned). `^` is a second negation
spelling in fnmatch() and an ordinary member in glob(), which is glibc's own split
between the two.

php on Windows answers from its own older glob() and fnmatch() -- no POSIX
classes, `[]]` read differently, a doubled slash under GLOB_MARK -- which is not
the contract pinned here, so the oracle pass skips there.
--SKIPIF--
<?php if (function_exists('zend_version') && PHP_OS_FAMILY === 'Windows') echo 'skip php on Windows has its own glob/fnmatch'; ?>
--FILE--
<?php
$glbDir = sys_get_temp_dir() . '/phl_glob_' . getmypid();
@mkdir($glbDir);
foreach (['a', 'b', 'a/x', 'a/y', 'b/z'] as $glbD) { @mkdir("$glbDir/$glbD"); }
foreach (['t.txt', 'u.dat', '.dot', 'a/1.txt', 'a/2.dat', 'a/.h',
          'a/x/3.txt', 'a/y/4.log', 'b/5.txt', 'b/z/6.txt'] as $glbF) {
    file_put_contents("$glbDir/$glbF", '');
}
function glbShow($pattern, $flags = 0) {
    global $glbDir;
    $out = glob("$glbDir/$pattern", $flags);
    if (is_array($out)) {
        /* GLOB_MARK appends the platform's own separator, so the marks are
         * normalised here: what this pins is WHICH names came back marked. */
        $out = array_map(fn($x) => str_replace('\\', '/', substr($x, strlen($glbDir) + 1)), $out);
    }
    echo str_pad($pattern, 16), ' fl=', str_pad((string)$flags, 10), ' => ',
         json_encode($out), "\n";
}

/* Every segment is matched, not just the last. */
glbShow('*.txt');
glbShow('*/*.txt');
glbShow('*/*/*.txt');
glbShow('*/*');
glbShow('?/1.txt');
glbShow('[ab]/*.txt');
glbShow('*/2.dat');
glbShow('*/x');
glbShow('nope/*');
glbShow('a/1.txt');
glbShow('*/*/*/*.txt');

/* A trailing slash names directories and is KEPT -- one slash at a time. */
glbShow('*/');
glbShow('a/');
glbShow('x*/');
glbShow('*/*/');
glbShow('*//');
glbShow('[ab]/');

/* The flags still describe the final answer. */
glbShow('*', GLOB_ONLYDIR);
glbShow('*', GLOB_MARK);
glbShow('*/', GLOB_MARK);
glbShow('*/*.txt', GLOB_ONLYDIR);
glbShow('*/zz', GLOB_NOCHECK);
glbShow('zz/', GLOB_NOCHECK);
glbShow('{a,b}/*.txt', GLOB_BRACE);

/* The set rules, through glob() -- where `^` is an ordinary member. */
glbShow('*[[:digit:]].txt');
glbShow('[!ab]*');
glbShow('*[tg]');
glbShow('[[:alpha:]]');
glbShow('[[:alpha:]]/[[:digit:]].*');

/* ...and through fnmatch(), where it is a second negation spelling. */
foreach ([['*[ab]', 'xa'], ['*[ab]', 'xc'], ['a*[0-9]', 'a12'], ['*[[:digit:]]', 'ab1'],
          ['*[!a]', 'xb'], ['*[!a]', 'xa'], ['[!a]', 'b'], ['[!a]', 'a'],
          ['[^a]', 'b'], ['[^a]', 'a'], ['[^a]', '^'], ['[!]a]', ']'], ['[!]]', 'a'],
          ['[[:alpha:]]', 'a'], ['[[:upper:]]', 'A'], ['[[:upper:]]', 'a'],
          ['[[:punct:]]', '.'], ['[[:space:]]', ' '], ['[[:xdigit:]]', 'f'],
          ['[[:nope:]]', 'n'], ['[a-c]', 'b'], ['[!a-c]', 'd'],
          ['[]]', ']'], ['a\\*', 'a*'], ['a\\*', 'ab']] as [$glbP, $glbS]) {
    echo str_pad(json_encode($glbP), 16), ' vs ', str_pad(json_encode($glbS), 6),
         ' plain=', var_export(fnmatch($glbP, $glbS), true),
         /* a character CLASS under FNM_CASEFOLD is the C library's answer --
          * glibc never folds one, BSD's fnmatch folds the string first -- so
          * that cell is not pinned */
         ' fold=', str_contains($glbP, '[:') ? '-' : var_export(fnmatch($glbP, $glbS, FNM_CASEFOLD), true),
         ' noesc=', var_export(fnmatch($glbP, $glbS, FNM_NOESCAPE), true), "\n";
}
/* FNM_CASEFOLD reaches members and ranges. */
foreach ([['[a-c]', 'B'], ['[A-C]', 'b'], ['[!a-c]', 'A'], ['[abc]', 'B']] as [$glbP, $glbS]) {
    echo str_pad(json_encode($glbP), 16), ' vs ', str_pad(json_encode($glbS), 6),
         ' plain=', var_export(fnmatch($glbP, $glbS), true),
         ' fold=', var_export(fnmatch($glbP, $glbS, FNM_CASEFOLD), true), "\n";
}

foreach (['a/x/3.txt', 'a/y/4.log', 'b/z/6.txt', 'a/1.txt', 'a/2.dat', 'a/.h',
          'b/5.txt', 't.txt', 'u.dat', '.dot'] as $glbF) { @unlink("$glbDir/$glbF"); }
foreach (['a/x', 'a/y', 'b/z', 'a', 'b'] as $glbD) { @rmdir("$glbDir/$glbD"); }
@rmdir($glbDir);
--EXPECT--
*.txt            fl=0          => ["t.txt"]
*/*.txt          fl=0          => ["a\/1.txt","b\/5.txt"]
*/*/*.txt        fl=0          => ["a\/x\/3.txt","b\/z\/6.txt"]
*/*              fl=0          => ["a\/1.txt","a\/2.dat","a\/x","a\/y","b\/5.txt","b\/z"]
?/1.txt          fl=0          => ["a\/1.txt"]
[ab]/*.txt       fl=0          => ["a\/1.txt","b\/5.txt"]
*/2.dat          fl=0          => ["a\/2.dat"]
*/x              fl=0          => ["a\/x"]
nope/*           fl=0          => []
a/1.txt          fl=0          => ["a\/1.txt"]
*/*/*/*.txt      fl=0          => []
*/               fl=0          => ["a\/","b\/"]
a/               fl=0          => ["a\/"]
x*/              fl=0          => []
*/*/             fl=0          => ["a\/x\/","a\/y\/","b\/z\/"]
*//              fl=0          => ["a\/\/","b\/\/"]
[ab]/            fl=0          => ["a\/","b\/"]
*                fl=1073741824 => ["a","b"]
*                fl=8          => ["a\/","b\/","t.txt","u.dat"]
*/               fl=8          => ["a\/","b\/"]
*/*.txt          fl=1073741824 => []
*/zz             fl=16         => ["*\/zz"]
zz/              fl=16         => ["zz\/"]
{a,b}/*.txt      fl=128        => ["a\/1.txt","b\/5.txt"]
*[[:digit:]].txt fl=0          => []
[!ab]*           fl=0          => ["t.txt","u.dat"]
*[tg]            fl=0          => ["t.txt","u.dat"]
[[:alpha:]]      fl=0          => ["a","b"]
[[:alpha:]]/[[:digit:]].* fl=0          => ["a\/1.txt","a\/2.dat","b\/5.txt"]
"*[ab]"          vs "xa"   plain=true fold=true noesc=true
"*[ab]"          vs "xc"   plain=false fold=false noesc=false
"a*[0-9]"        vs "a12"  plain=true fold=true noesc=true
"*[[:digit:]]"   vs "ab1"  plain=true fold=- noesc=true
"*[!a]"          vs "xb"   plain=true fold=true noesc=true
"*[!a]"          vs "xa"   plain=false fold=false noesc=false
"[!a]"           vs "b"    plain=true fold=true noesc=true
"[!a]"           vs "a"    plain=false fold=false noesc=false
"[^a]"           vs "b"    plain=true fold=true noesc=true
"[^a]"           vs "a"    plain=false fold=false noesc=false
"[^a]"           vs "^"    plain=true fold=true noesc=true
"[!]a]"          vs "]"    plain=false fold=false noesc=false
"[!]]"           vs "a"    plain=true fold=true noesc=true
"[[:alpha:]]"    vs "a"    plain=true fold=- noesc=true
"[[:upper:]]"    vs "A"    plain=true fold=- noesc=true
"[[:upper:]]"    vs "a"    plain=false fold=- noesc=false
"[[:punct:]]"    vs "."    plain=true fold=- noesc=true
"[[:space:]]"    vs " "    plain=true fold=- noesc=true
"[[:xdigit:]]"   vs "f"    plain=true fold=- noesc=true
"[[:nope:]]"     vs "n"    plain=false fold=- noesc=false
"[a-c]"          vs "b"    plain=true fold=true noesc=true
"[!a-c]"         vs "d"    plain=true fold=true noesc=true
"[]]"            vs "]"    plain=true fold=true noesc=true
"a\\*"           vs "a*"   plain=true fold=true noesc=false
"a\\*"           vs "ab"   plain=false fold=false noesc=false
"[a-c]"          vs "B"    plain=false fold=true
"[A-C]"          vs "b"    plain=false fold=true
"[!a-c]"         vs "A"    plain=true fold=false
"[abc]"          vs "B"    plain=false fold=true
