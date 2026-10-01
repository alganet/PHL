--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini: a TAB ends a name, and `name[` opens an offset that has to close and take an `=`
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
// Three productions compete where php's scanner reads a directive name, and
// which blank run each of them eats is what tells them apart. A `{LABEL}` stops
// dead at a TAB; `{LABEL}"["` opens an offset and swallows a spaces-only run in
// front of it; and only a run holding a TAB reaches the rule that throws a
// blank run away. So `\t[s]` is a section header and `  [s]` is an offset.
$files = array(
    // A TAB ends the name and the statement with it -- the run in front of it
    // is a bare label php's php.ini callback drops, and what follows is read as
    // a statement of its own. Only blanks-then-`=` outrun that.
    'a tab ends the name'          => "user_agent=a\nxx\tprecision=9\nfrom=b\n",
    'blanks then `=` do not'       => "user_agent=a\nprecision\t= 9\nfrom=b\n",
    'a tab inside a name'          => "user_agent=a\nprec\tision=9\nfrom=b\n",
    'and it can open an offset'    => "user_agent=a\nxx\tfoo[bar]]=1\nfrom=b\n",
    // `TC_OFFSET option_offset ']' '='` is the only statement php builds out of
    // an offset, so the run has to meet its `]` and an `=` has to follow it.
    'an offset that closes'        => "user_agent=a\nfoo[bar]=1\nfrom=b\n",
    'blanks inside the brackets'   => "user_agent=a\nfoo[ bar ]=1\nfrom=b\n",
    'a bool word opens one'        => "user_agent=a\non[x]=1\nfrom=b\n",
    'a `]` that never comes'       => "user_agent=a\nab[c=1\nfrom=b\n",
    'a newline does not close it'  => "user_agent=a\nfoo[bar\nfrom=b\n",
    'nor does a `;`'               => "user_agent=a\nfoo[a;b]=1\nfrom=b\n",
    'a backslash carries one'      => "user_agent=a\nfoo[a\\\nb]=1\nfrom=b\n",
    'an unclosed quote inside'     => "user_agent=a\nfoo[\"bar\nfrom=b\n",
    // Behind the `]` php is back at a statement position with nothing but an
    // `=` acceptable, and it names every other token the way that state does.
    'a `]` behind the offset'      => "user_agent=a\nfoo[bar]]=1\nfrom=b\n",
    'a word behind it'             => "user_agent=a\nfoo[bar]x\nfrom=b\n",
    'a bool word behind it'        => "user_agent=a\nfoo[bar]on\nfrom=b\n",
    'a second offset behind it'    => "user_agent=a\nfoo[bar]on[x]=1\nfrom=b\n",
    'a `[` behind it'              => "user_agent=a\nfoo[bar][\nfrom=b\n",
    'a `~` behind it'              => "user_agent=a\nfoo[bar]~\nfrom=b\n",
    'nothing behind it'            => "user_agent=a\nfoo[bar]\nfrom=b\n",
    'blanks behind it'             => "user_agent=a\nfoo[bar]  \nfrom=b\n",
    'a comment behind it'          => "user_agent=a\nfoo[bar];c\nfrom=b\n",
    'and no newline behind it'     => "user_agent=a\nfoo[bar]",
    // The blank run in front of the bracket decides which of the two it is.
    'spaces make it an offset'     => "user_agent=a\n  [s]\nfrom=b\n",
    'and one that can be set'      => "user_agent=a\n  [s]=1\nfrom=b\n",
    'a tab leaves it a section'    => "user_agent=a\n\t[s]\nfrom=b\n",
    'and so does a mixed run'      => "user_agent=a\n\t  [s]\nfrom=b\n",
    // The LABEL run has to reach the bracket for an offset to form at all.
    'an `&` gets there first'      => "user_agent=a\nx&y[z]=1\nfrom=b\n",
    // A section name is the same run as an offset's, so the same three things
    // close it and the same double-quoted run moves php's line counter.
    'a `;` inside a header'        => "user_agent=a\n[a;b]\nfrom=b\n",
    'an empty raw string in one'   => "user_agent=a\n[a''b]\nfrom=b\n",
    'a quoted newline dates it'    => "user_agent=a\n[a \"x\ny\" b\nfrom=b\n",
);
foreach ($files as $label => $text) {
    file_put_contents($ini, $text);
    echo "## $label\n";
    echo ini_run("\"$phl\" -c \"$ini\" -r '$show'", $ini), "\n";
}
@unlink($ini);
?>
--EXPECT--
## a tab ends the name
user_agent=[a]
from=[b]
precision=[9]
## blanks then `=` do not
user_agent=[a]
from=[b]
precision=[9]
## a tab inside a name
user_agent=[a]
from=[b]
precision=[14]
## and it can open an offset
PHP:  syntax error, unexpected ']', expecting '=' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## an offset that closes
user_agent=[a]
from=[b]
precision=[14]
## blanks inside the brackets
user_agent=[a]
from=[b]
precision=[14]
## a bool word opens one
user_agent=[a]
from=[b]
precision=[14]
## a `]` that never comes
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a newline does not close it
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## nor does a `;`
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a backslash carries one
user_agent=[a]
from=[b]
precision=[14]
## an unclosed quote inside
PHP:  syntax error, unexpected end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '"' in INI on line 4
user_agent=[a]
from=[]
precision=[14]
## a `]` behind the offset
PHP:  syntax error, unexpected ']', expecting '=' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a word behind it
PHP:  syntax error, unexpected TC_LABEL, expecting '=' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a bool word behind it
PHP:  syntax error, unexpected BOOL_TRUE, expecting '=' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a second offset behind it
PHP:  syntax error, unexpected TC_OFFSET, expecting '=' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `[` behind it
PHP:  syntax error, unexpected TC_SECTION, expecting '=' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `~` behind it
PHP:  syntax error, unexpected '~', expecting '=' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## nothing behind it
PHP:  syntax error, unexpected END_OF_LINE, expecting '=' in INI on line 3
user_agent=[a]
from=[]
precision=[14]
## blanks behind it
PHP:  syntax error, unexpected END_OF_LINE, expecting '=' in INI on line 3
user_agent=[a]
from=[]
precision=[14]
## a comment behind it
PHP:  syntax error, unexpected END_OF_LINE, expecting '=' in INI on line 3
user_agent=[a]
from=[]
precision=[14]
## and no newline behind it
PHP:  syntax error, unexpected end of file, expecting '=' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## spaces make it an offset
PHP:  syntax error, unexpected END_OF_LINE, expecting '=' in INI on line 3
user_agent=[a]
from=[]
precision=[14]
## and one that can be set
user_agent=[a]
from=[b]
precision=[14]
## a tab leaves it a section
user_agent=[a]
from=[b]
precision=[14]
## and so does a mixed run
user_agent=[a]
from=[b]
precision=[14]
## an `&` gets there first
PHP:  syntax error, unexpected '&' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `;` inside a header
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## an empty raw string in one
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a quoted newline dates it
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 3
user_agent=[a]
from=[]
precision=[14]
