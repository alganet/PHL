--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini: what stands where a directive name goes is a TOKEN, and a dozen of them refuse the file
--SKIPIF--
<?php
// Spawns the interpreter through popen() with single-quoted `-r '...'` arguments —
// POSIX shell quoting that cmd.exe does not honour (php fails this on Windows too).
if (DIRECTORY_SEPARATOR === '\\') { echo 'skip POSIX shell quoting in the subprocess harness'; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$ini = tempnam(sys_get_temp_dir(), 'phlini');
$show = 'foreach (array("user_agent","from","precision") as $n) '
      . 'printf("%s=[%s]\n", $n, ini_get($n));';
function ini_run($cmd, $ini) {
    $fp = popen($cmd . ' 2>&1', 'r');
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    pclose($fp);
    // a diagnostic names the file with its symlinks resolved (macOS /private)
    return str_replace([realpath($ini), $ini], 'INI', trim($out));
}
// A directive NAME is not "everything up to the `=`". php's scanner reads a
// {LABEL} run, which stops at every byte its operators, brackets and line ends
// are made of, and each of those has a rule of its own behind it: a `;` opens a
// comment and a `[` an offset, but the twelve below are tokens no statement of
// its grammar starts with, so meeting one refuses the file from there down --
// whether it opens the name or sits in the middle of one. And the eight bool
// words have a rule AHEAD of the one that reads a LABEL, so `on = 1` is a
// refusal where `onx = 1` is the entry "onx".
$files = array(
    'a stray `]`'                  => "user_agent=a\n]\nfrom=b\n",
    'in the middle of a name'      => "user_agent=a\nx]y=1\nfrom=b\n",
    'an `&`'                       => "user_agent=a\n&\nfrom=b\n",
    'a `|`'                        => "user_agent=a\nx|y=1\nfrom=b\n",
    'a `^`'                        => "user_agent=a\n^\nfrom=b\n",
    'a `~`'                        => "user_agent=a\n~\nfrom=b\n",
    'a `$`'                        => "user_agent=a\n\$\nfrom=b\n",
    'a `(`'                        => "user_agent=a\n(\nfrom=b\n",
    'a `)`'                        => "user_agent=a\n)\nfrom=b\n",
    'a `{`'                        => "user_agent=a\n{\nfrom=b\n",
    'a `}`'                        => "user_agent=a\n}\nfrom=b\n",
    'a `!`'                        => "user_agent=a\n!\nfrom=b\n",
    'a quoted name'                => "user_agent=a\n\"x\"=1\nfrom=b\n",
    'an `=` with no name'          => "user_agent=a\n=1\nfrom=b\n",
    'blanks do not hide it'        => "user_agent=a\n   ]\nfrom=b\n",
    'nor does a tab'               => "user_agent=a\n\t]\nfrom=b\n",
    'a `.` is an ordinary name'    => "user_agent=a\nx.y=1\nfrom=b\n",
    'and so are `:` `\\` `%` `#`'  => "user_agent=a\nx:y\\z%w#v=1\nfrom=b\n",
    'a `;` opens a comment'        => "user_agent=a\nx;]=1\nfrom=b\n",
    'a `[` opens an offset'        => "user_agent=a\nx[y]=1\nfrom=b\n",
    // A bool word has to OPEN the run, and its trailing blanks are what let it
    // outrun a {LABEL}, which stops dead at a TAB. flex takes the longest match
    // and, on a tie, the earliest rule.
    'the word `on`'                => "user_agent=a\non = 1\nfrom=b\n",
    'the word `off`'               => "user_agent=a\noff=1\nfrom=b\n",
    'the word `null`'              => "user_agent=a\nnull=1\nfrom=b\n",
    'the word `none`'              => "user_agent=a\nnone=1\nfrom=b\n",
    'the word `yes`'               => "user_agent=a\nyes=1\nfrom=b\n",
    'the word `true`'              => "user_agent=a\ntrue=1\nfrom=b\n",
    'case does not save it'        => "user_agent=a\nON=1\nfrom=b\n",
    'a tab does not either'        => "user_agent=a\non\t=1\nfrom=b\n",
    'but a longer run does'        => "user_agent=a\nonx=1\nfrom=b\n",
    'and so does a second word'    => "user_agent=a\non x = 1\nfrom=b\n",
    'the word must open the run'   => "user_agent=a\nxon=1\nfrom=b\n",
    // php's php.ini callback ignores a statement carrying no value, so a bare
    // name neither defines the entry nor sets it to "1".
    'a bare name sets nothing'     => "precision\nfrom=b\n",
    // The `]` that closes a section header eats the blanks behind it, eats a
    // newline when one is there, and counts a line either way -- so a directive
    // written on the header's own line is read, and a refusal below a header
    // that did not end its own line is dated one line lower than it was typed.
    'a directive behind a header'  => "[s] precision=9\nfrom=b\n",
    'a `]` behind a header'        => "user_agent=a\n[s]]\nfrom=b\n",
    'the header ate the newline'   => "user_agent=a\n[s]\n]\nfrom=b\n",
    'and one line of drift'        => "user_agent=a\n[s] junk\n]\nfrom=b\n",
);
foreach ($files as $label => $text) {
    file_put_contents($ini, $text);
    echo "## $label\n";
    echo ini_run("\"$phl\" -c \"$ini\" -r '$show'", $ini), "\n";
}
@unlink($ini);
// One -d is one statement of that same buffer, and a refusal there takes the
// -d's behind it while leaving a -c file alone.
echo "## a bool word as a -d\n";
echo ini_run("\"$phl\" -d " . escapeshellarg('on=1')
    . ' -d ' . escapeshellarg('from=b') . " -r '$show'", $ini), "\n";
?>
--EXPECT--
## a stray `]`
PHP:  syntax error, unexpected ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## in the middle of a name
PHP:  syntax error, unexpected ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## an `&`
PHP:  syntax error, unexpected '&' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `|`
PHP:  syntax error, unexpected '|' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `^`
PHP:  syntax error, unexpected '^' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `~`
PHP:  syntax error, unexpected '~' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `$`
PHP:  syntax error, unexpected '$' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `(`
PHP:  syntax error, unexpected '(' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `)`
PHP:  syntax error, unexpected ')' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `{`
PHP:  syntax error, unexpected '{' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `}`
PHP:  syntax error, unexpected '}' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `!`
PHP:  syntax error, unexpected '!' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a quoted name
PHP:  syntax error, unexpected '"' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## an `=` with no name
PHP:  syntax error, unexpected '=' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## blanks do not hide it
PHP:  syntax error, unexpected ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## nor does a tab
PHP:  syntax error, unexpected ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `.` is an ordinary name
user_agent=[a]
from=[b]
precision=[14]
## and so are `:` `\` `%` `#`
user_agent=[a]
from=[b]
precision=[14]
## a `;` opens a comment
user_agent=[a]
from=[b]
precision=[14]
## a `[` opens an offset
user_agent=[a]
from=[b]
precision=[14]
## the word `on`
PHP:  syntax error, unexpected BOOL_TRUE in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## the word `off`
PHP:  syntax error, unexpected BOOL_FALSE in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## the word `null`
PHP:  syntax error, unexpected NULL_NULL in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## the word `none`
PHP:  syntax error, unexpected BOOL_FALSE in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## the word `yes`
PHP:  syntax error, unexpected BOOL_TRUE in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## the word `true`
PHP:  syntax error, unexpected BOOL_TRUE in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## case does not save it
PHP:  syntax error, unexpected BOOL_TRUE in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a tab does not either
PHP:  syntax error, unexpected BOOL_TRUE in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## but a longer run does
user_agent=[a]
from=[b]
precision=[14]
## and so does a second word
user_agent=[a]
from=[b]
precision=[14]
## the word must open the run
user_agent=[a]
from=[b]
precision=[14]
## a bare name sets nothing
user_agent=[]
from=[b]
precision=[14]
## a directive behind a header
user_agent=[]
from=[b]
precision=[9]
## a `]` behind a header
PHP:  syntax error, unexpected ']' in INI on line 3
user_agent=[a]
from=[]
precision=[14]
## the header ate the newline
PHP:  syntax error, unexpected ']' in INI on line 3
user_agent=[a]
from=[]
precision=[14]
## and one line of drift
PHP:  syntax error, unexpected ']' in INI on line 4
user_agent=[a]
from=[]
precision=[14]
## a bool word as a -d
PHP:  syntax error, unexpected BOOL_TRUE in Unknown on line 6
user_agent=[]
from=[]
precision=[14]
