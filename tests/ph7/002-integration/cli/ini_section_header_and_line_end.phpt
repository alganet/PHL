--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini: a `[` with no `]` on its line refuses the file, and a value's END_OF_LINE needs a newline
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
// A section NAME is one scanner run and only the `]` closes it. A newline does
// not, so a `[` with no `]` behind it on its own line runs out of SOURCE: the
// refusal is `end of file, expecting ']'`, dated where the run stopped, and it
// takes down every directive below while leaving the ones above standing. A `]`
// written further down does not rescue it, and neither does a later section.
// Inside the name a double-quoted run hides a `]` and crosses newlines, counting
// them; a raw `'` run and a `${}` hide one without counting; and a backslash
// carries the byte behind it, a newline included.
$files = array(
    'a closed section is skipped'   => "user_agent=a\n[sec]\nfrom=b\n",
    'no `]` at all'                 => "user_agent=a\n[sec\nfrom=b\n",
    'a bare `[`'                    => "user_agent=a\n[\nfrom=b\n",
    'a `]` on a later line'         => "user_agent=a\n[sec\nfrom=b\n]\nprecision=9\n",
    'a later section does not help' => "user_agent=a\n[sec\n[other]\nfrom=b\n",
    'the last section is the one'   => "user_agent=a\n[s]\nfrom=b\n[t\nprecision=9\n",
    'the `[` ends the file'         => "user_agent=a\n[sec",
    'on the first line'             => "[sec\nuser_agent=a\n",
    'indented all the same'         => "user_agent=a\n  [sec\nfrom=b\n",
    'blanks inside the brackets'    => "user_agent=a\n[ sec ]\nfrom=b\n",
    'text behind the `]`'           => "user_agent=a\n[sec] junk\nfrom=b\n",
    'a comment behind the `]`'      => "user_agent=a\n[sec];c\nfrom=b\n",
    'a quoted `]` is not the end'   => "user_agent=a\n[\"s]\"]\nfrom=b\n",
    'a quoted run crosses lines'    => "user_agent=a\n[\"sec\nx\"]\nfrom=b\n",
    'and counts them'               => "user_agent=a\n[\"sec\nx\"]\nprecision=1 &\n",
    'an unclosed quote wants more'  => "user_agent=a\n[s\"x]\nfrom=b\n",
    'a raw `]` is not the end'      => "user_agent=a\n['s]']\nfrom=b\n",
    'a raw run crosses lines'       => "user_agent=a\n['s\nx']\nfrom=b\n",
    'without counting them'         => "user_agent=a\n['s\nx']\nprecision=1 &\n",
    'an unclosed raw run'           => "user_agent=a\n[s'x]\nfrom=b\n",
    'an unclosed `${` wants a name' => "user_agent=a\n[s\${\nfrom=b\n",
    'a backslash takes the newline' => "user_agent=a\n[s\\\n]\nprecision=1 &\n",
    // A value that merely ran out is END_OF_LINE, and php's NEWLINE rule counts
    // the line it has just eaten — so the refusal is dated one line BELOW the
    // directive, but only when there was a newline there to eat. A source whose
    // last line has none stays on the directive's own line, and a `;` comment
    // left hanging at the end of the input matches no rule at all, which makes
    // what the parser is handed the end of the file rather than a line's end.
    'a newline is one line down'    => "user_agent=1 &\n",
    'no newline stays put'          => "user_agent=1 &",
    'trailing blanks are not one'   => "user_agent=1 &   ",
    'a CRLF counts once'            => "user_agent=1 &\r\n",
    'a comment then a newline'      => "user_agent=1 & ; c\n",
    'a comment at the end of file'  => "user_agent=1 & ; c",
    'an empty comment, no newline'  => "user_agent=1 & ;",
    'a comment with no blank'       => "user_agent=1 &;c",
    'the second line, no newline'   => "from=x\nuser_agent=1 &",
    'a quoted run moved it first'   => "user_agent=\"a\nb\" &",
    'a real token is not moved'     => "user_agent=1 & )",
    'not even behind a comment'     => "user_agent=1 & ) ;c",
);
foreach ($files as $label => $text) {
    file_put_contents($ini, $text);
    echo "## $label\n";
    echo ini_run("\"$phl\" -c \"$ini\" -r '$show'", $ini), "\n";
}
@unlink($ini);
// The CLI writes every -d into one buffer with a newline behind each, so a -d
// value always has that newline to be dated by.
echo "## -d always has its newline\n";
echo ini_run("\"$phl\" -d " . escapeshellarg('user_agent=1 &') . " -r '$show'", $ini), "\n";
?>
--EXPECT--
## a closed section is skipped
user_agent=[a]
from=[b]
precision=[14]
## no `]` at all
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a bare `[`
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a `]` on a later line
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a later section does not help
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## the last section is the one
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 4
user_agent=[a]
from=[b]
precision=[14]
## the `[` ends the file
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## on the first line
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 1
user_agent=[]
from=[]
precision=[14]
## indented all the same
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## blanks inside the brackets
user_agent=[a]
from=[b]
precision=[14]
## text behind the `]`
user_agent=[a]
from=[b]
precision=[14]
## a comment behind the `]`
user_agent=[a]
from=[b]
precision=[14]
## a quoted `]` is not the end
user_agent=[a]
from=[b]
precision=[14]
## a quoted run crosses lines
user_agent=[a]
from=[b]
precision=[14]
## and counts them
PHP:  syntax error, unexpected END_OF_LINE in INI on line 5
user_agent=[a]
from=[]
precision=[14]
## an unclosed quote wants more
PHP:  syntax error, unexpected end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '"' in INI on line 4
user_agent=[a]
from=[]
precision=[14]
## a raw `]` is not the end
user_agent=[a]
from=[b]
precision=[14]
## a raw run crosses lines
user_agent=[a]
from=[b]
precision=[14]
## without counting them
PHP:  syntax error, unexpected END_OF_LINE in INI on line 4
user_agent=[a]
from=[]
precision=[14]
## an unclosed raw run
PHP:  syntax error, unexpected end of file, expecting ']' in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## an unclosed `${` wants a name
PHP:  syntax error, unexpected end of file, expecting TC_VARNAME in INI on line 2
user_agent=[a]
from=[]
precision=[14]
## a backslash takes the newline
PHP:  syntax error, unexpected END_OF_LINE in INI on line 4
user_agent=[a]
from=[]
precision=[14]
## a newline is one line down
PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
user_agent=[]
from=[]
precision=[14]
## no newline stays put
PHP:  syntax error, unexpected END_OF_LINE in INI on line 1
user_agent=[]
from=[]
precision=[14]
## trailing blanks are not one
PHP:  syntax error, unexpected END_OF_LINE in INI on line 1
user_agent=[]
from=[]
precision=[14]
## a CRLF counts once
PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
user_agent=[]
from=[]
precision=[14]
## a comment then a newline
PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
user_agent=[]
from=[]
precision=[14]
## a comment at the end of file
PHP:  syntax error, unexpected end of file in INI on line 1
user_agent=[]
from=[]
precision=[14]
## an empty comment, no newline
PHP:  syntax error, unexpected end of file in INI on line 1
user_agent=[]
from=[]
precision=[14]
## a comment with no blank
PHP:  syntax error, unexpected end of file in INI on line 1
user_agent=[]
from=[]
precision=[14]
## the second line, no newline
PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
user_agent=[]
from=[x]
precision=[14]
## a quoted run moved it first
PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
user_agent=[]
from=[]
precision=[14]
## a real token is not moved
PHP:  syntax error, unexpected ')' in INI on line 1
user_agent=[]
from=[]
precision=[14]
## not even behind a comment
PHP:  syntax error, unexpected ')' in INI on line 1
user_agent=[]
from=[]
precision=[14]
## -d always has its newline
PHP:  syntax error, unexpected END_OF_LINE in Unknown on line 7
user_agent=[]
from=[]
precision=[14]
