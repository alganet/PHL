--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini: a value ending on `$` carries the newline and runs on into the next line
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
// A value's VALUE_CHARS unit is `("$"[^{])`: the `$` carries the byte behind it
// whatever that byte is, and one more when it is a backslash. The newline is not
// special to it, so a value whose last byte is `$` does not end at its own line —
// it eats the line break and keeps scanning the text below, which is how a value
// reaches the next directive's `=`. Two things follow that are easy to get wrong:
// the line counter does NOT move for a newline the value ate (only the scanner's
// own NEWLINE rule counts), so a refusal below a run-on is dated one line short of
// where it was typed; and a `$` with nothing behind it matches no unit at all, so
// a value that ends the file on one simply stops in front of it.
$files = array(
    'runs on to the next `=`'   => "user_agent=a\$\nfrom=zzz\n",
    'runs on over a bare line'  => "user_agent=a\$\nzzz\nprecision=9\n",
    'eats two line breaks'      => "user_agent=a\$\nb\$\nc\nprecision=9\n",
    'the eaten line is not counted' => "user_agent=a\$\nzzz\nprecision=1 & )\n",
    'a quoted run does count it' => "user_agent=\"a\nb\"\nprecision=1 & )\n",
    'the `$` takes a `;`'       => "user_agent=a\$;c\nfrom=zzz\n",
    'the `$` takes a backslash and one more' => "user_agent=a\$\\b\nfrom=zzz\n",
    '`\${` is its own production' => "user_agent=\${PHL_NO_SUCH_VAR}x\nfrom=zzz\n",
    'nothing behind it at EOF'  => "user_agent=a\$",
    'a lone `$` still runs on'  => "user_agent=\$\nfrom=zzz\n",
    'a bare `=` is not a value char' => "user_agent=a=b\nfrom=zzz\n",
);
foreach ($files as $label => $text) {
    file_put_contents($ini, $text);
    echo "## $label\n";
    echo ini_run("\"$phl\" -c \"$ini\" -r '$show'", $ini), "\n";
}
@unlink($ini);
// The CLI writes every -d into ONE buffer, a newline behind each, so the same
// unit carries a value out of its own -d and into the next one's text — and the
// newline php's own builder appended is part of the value it stores.
$defs = array(
    'the appended newline is the value\'s' => array('user_agent=a$'),
    'runs on into the next -d'  => array('user_agent=a$', 'from=zzz'),
    'a `$` inside a value'      => array('user_agent=a$b'),
);
foreach ($defs as $label => $argv) {
    $cmd = "\"$phl\"";
    foreach ($argv as $one) { $cmd .= ' -d ' . escapeshellarg($one); }
    echo "## $label\n";
    echo ini_run($cmd . " -r '$show'", $ini), "\n";
}
?>
--EXPECT--
## runs on to the next `=`
PHP:  syntax error, unexpected '=' in INI on line 1
user_agent=[a$
from]
from=[]
precision=[14]
## runs on over a bare line
user_agent=[a$
zzz]
from=[]
precision=[9]
## eats two line breaks
user_agent=[a$
b$
c]
from=[]
precision=[9]
## the eaten line is not counted
PHP:  syntax error, unexpected ')' in INI on line 2
user_agent=[a$
zzz]
from=[]
precision=[14]
## a quoted run does count it
PHP:  syntax error, unexpected ')' in INI on line 3
user_agent=[a
b]
from=[]
precision=[14]
## the `$` takes a `;`
user_agent=[a$;c]
from=[zzz]
precision=[14]
## the `$` takes a backslash and one more
user_agent=[a$\b]
from=[zzz]
precision=[14]
## `\${` is its own production
user_agent=[x]
from=[zzz]
precision=[14]
## nothing behind it at EOF
user_agent=[a]
from=[]
precision=[14]
## a lone `$` still runs on
PHP:  syntax error, unexpected '=' in INI on line 1
user_agent=[$
from]
from=[]
precision=[14]
## a bare `=` is not a value char
PHP:  syntax error, unexpected '=' in INI on line 1
user_agent=[a]
from=[]
precision=[14]
## the appended newline is the value's
user_agent=[a$
]
from=[]
precision=[14]
## runs on into the next -d
PHP:  syntax error, unexpected '=' in Unknown on line 6
user_agent=[a$
from]
from=[]
precision=[14]
## a `$` inside a value
user_agent=[a$b]
from=[]
precision=[14]
