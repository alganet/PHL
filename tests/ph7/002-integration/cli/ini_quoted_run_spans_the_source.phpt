--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini: a quoted run belongs to the source, not to the line it opened on
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
$show = 'foreach (array("user_agent","from","default_mimetype") as $n) '
      . 'printf("%s=[%s]\n", $n, ini_get($n));';
function ini_run($cmd, $ini) {
    $fp = popen($cmd . ' 2>&1', 'r');
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    pclose($fp);
    // a diagnostic names the file with its symlinks resolved (macOS /private)
    return str_replace([realpath($ini), $ini], 'INI', trim($out));
}
// A quoted run is a scanner STATE, so it keeps going past the newline and the
// directives behind it are its content. The two quotes differ in three ways:
// the raw string is one match that never touches the line counter, the
// double-quoted run counts every newline it eats, and only the double-quoted
// run names the tokens it was still willing to take when it ran out.
$files = array(
    'raw spans and closes'      => "user_agent='abc\ndef'\nfrom=)\n",
    'double spans and closes'   => "user_agent=\"abc\ndef\"\nfrom=)\n",
    'leftover after a raw run'  => "user_agent='a\nb')\nfrom=f\n",
    'leftover after a double run'   => "user_agent=\"a\nb\")\nfrom=f\n",
    'double never closes'       => "user_agent=\"abc\nfrom=f\ndefault_mimetype=text/z\n",
    'double after a value'      => "user_agent=A\"b\nfrom=f\n",
    'raw never closes'          => "user_agent=UA\nfrom='abc\ndefault_mimetype=text/z\n",
    'raw eats a whole value'    => "user_agent=(1)'\nfrom=f\ndefault_mimetype=text/z\n",
    'raw eats a bare string'    => "user_agent=A'b\nfrom=f\n",
    'raw closes a line later'   => "user_agent=(1)'x\nfrom='y\ndefault_mimetype=text/z\n",
);
foreach ($files as $label => $text) {
    file_put_contents($ini, $text);
    echo "## $label\n";
    echo ini_run("\"$phl\" -c \"$ini\" -r '$show'", $ini), "\n";
}
@unlink($ini);
// Every -d is ONE scanner input too: php's CLI joins them into a single buffer
// behind five lines of its own hardcoded startup ini, so a run one -d opens
// carries into the next one's text and a refusal is dated inside that buffer.
$defs = array(
    'unterminated eats them all' => array('user_agent="abc', 'from=x', 'default_mimetype=text/z'),
    'raw closes in the next -d'  => array("user_agent='abc", "from=x'y", 'default_mimetype=text/z'),
);
foreach ($defs as $label => $argv) {
    $cmd = "\"$phl\"";
    foreach ($argv as $one) { $cmd .= ' -d ' . escapeshellarg($one); }
    echo "## $label\n";
    echo ini_run($cmd . " -r '$show'", $ini), "\n";
}
?>
--EXPECT--
## raw spans and closes
PHP:  syntax error, unexpected ')' in INI on line 2
user_agent=[abc
def]
from=[]
default_mimetype=[text/html]
## double spans and closes
PHP:  syntax error, unexpected ')' in INI on line 3
user_agent=[abc
def]
from=[]
default_mimetype=[text/html]
## leftover after a raw run
PHP:  syntax error, unexpected ')' in INI on line 1
user_agent=[a
b]
from=[]
default_mimetype=[text/html]
## leftover after a double run
PHP:  syntax error, unexpected ')' in INI on line 2
user_agent=[a
b]
from=[]
default_mimetype=[text/html]
## double never closes
PHP:  syntax error, unexpected end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '"' in INI on line 4
user_agent=[]
from=[]
default_mimetype=[text/html]
## double after a value
PHP:  syntax error, unexpected end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '"' in INI on line 3
user_agent=[]
from=[]
default_mimetype=[text/html]
## raw never closes
PHP:  syntax error, unexpected end of file in INI on line 2
user_agent=[UA]
from=[]
default_mimetype=[text/html]
## raw eats a whole value
user_agent=[1]
from=[]
default_mimetype=[text/html]
## raw eats a bare string
user_agent=[A]
from=[]
default_mimetype=[text/html]
## raw closes a line later
PHP:  syntax error, unexpected TC_RAW in INI on line 1
user_agent=[1]
from=[]
default_mimetype=[text/html]
## unterminated eats them all
PHP:  syntax error, unexpected end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '"' in Unknown on line 9
user_agent=[]
from=[]
default_mimetype=[text/html]
## raw closes in the next -d
user_agent=[abc
from=xy]
from=[]
default_mimetype=[text/z]
