--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini: a `${` inside a section name is the value's substitution, refusals and all
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
$show = 'foreach (array("precision","from") as $n) '
      . 'printf("%s=[%s]\n", $n, ini_get($n));';
function ini_run($cmd, $ini) {
    $fp = popen($cmd . ' 2>&1', 'r');
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    pclose($fp);
    // a diagnostic names the file with its symlinks resolved (macOS /private)
    return str_replace([realpath($ini), $ini], 'INI', trim($out));
}
// php pushes ONE variable state, and it pushes it from the section state, from
// the offset state and from the value state alike. So `[${...}]` is not a name
// with a `}` somewhere in it: it is the value grammar's `${` run, read by the
// same rule and refused under the same tokens, and each of its four refusals
// carries an expect-list of its own. The one that reads like a bug in php is
// real: its scanner takes a variable name one LABEL_CHAR at a time, and a name
// exactly ONE character long followed by `:-` is swallowed by the rule that
// returns the fallback token — so `${A:-x}` is a syntax error where `${AB:-x}`
// is an ordinary substitution, in a section and in a value both.
//
// A refused `${` takes the source down from there the way any other ini error
// does, and a `${` php READS says nothing at all: a section header is walked
// for its extent, and stores no directive either way.
$files = array(
    'a name php reads'          => "[\${PHLV}]\nprecision=9\n",
    'a one-letter name loses to the fallback rule' => "[\${A:-x}]\nprecision=9\n",
    'a two-letter name does not' => "[\${AB:-x}]\nprecision=9\n",
    'no name at all'            => "[\${}]\nprecision=9\n",
    'nothing behind the brace'  => "[\${]\nprecision=9\n",
    'the name ends on a byte the label cannot hold' => "[\${AB=C}]\nprecision=9\n",
    'the name is all there is'  => "[\${AB]\nprecision=9\n",
    'a newline ends the name'   => "[\${AB\n}]\nprecision=9\n",
    'the fallback never closes' => "[\${AB:-x]\nprecision=9\n",
    'a `;` ends the fallback'   => "[\${AB:-x;}]\nprecision=9\n",
    'a raw quote ends it too'   => "[\${AB:-x'}]\nprecision=9\n",
    'a quote with no partner inside the fallback' => "[\${AB:-\"x}]\nprecision=9\n",
    'and behind its own text'   => "[\${AB:-x\"}]\nprecision=9\n",
    'a quoted run that closes'  => "[\${AB:-\"x\"}]\nprecision=9\n",
    'a quoted run counts its newlines' => "[\${AB:-\"a\nb\"c]\nprecision=9\n",
    'an escaped brace is not the end' => "[\${AB:-a\\}b}]\nprecision=9\n",
    'a nested run refuses too'  => "[\${AB:-\${}}]\nprecision=9\n",
    'a nested run that reads'   => "[\${AB:-\${PHLV}}]\nprecision=9\n",
    'the second run is screened too' => "[\${AB}\${}]\nprecision=9\n",
    'the run beats the `]` it never reaches' => "[\${AB:-x\nprecision=9\n",
    'the name goes on around it' => "[s\${AB:-x}t]\nprecision=9\n",
    'a directive above it stands' => "precision=9\n[\${A:-x}]\nfrom=zzz\n",
    'in an offset name'         => "x[\${A:-x}]=1\nprecision=9\n",
    'an offset name php reads'  => "x[\${PHLV}]=1\nprecision=9\n",
    'in a value, the same rule' => "precision=\${A:-x}\n",
    'a value fallback quote with no partner' => "from=\${AB:-\"x}\nprecision=9\n",
    'and behind its text'       => "from=\${AB:-x\"}\nprecision=9\n",
);
foreach ($files as $label => $text) {
    file_put_contents($ini, $text);
    echo "## $label\n";
    echo ini_run("\"$phl\" -c \"$ini\" -r '$show'", $ini), "\n";
}
@unlink($ini);
?>
--EXPECT--
## a name php reads
precision=[9]
from=[]
## a one-letter name loses to the fallback rule
PHP:  syntax error, unexpected TC_FALLBACK, expecting TC_VARNAME in INI on line 1
precision=[14]
from=[]
## a two-letter name does not
precision=[9]
from=[]
## no name at all
PHP:  syntax error, unexpected '}', expecting TC_VARNAME in INI on line 1
precision=[14]
from=[]
## nothing behind the brace
PHP:  syntax error, unexpected end of file, expecting TC_VARNAME in INI on line 1
precision=[14]
from=[]
## the name ends on a byte the label cannot hold
PHP:  syntax error, unexpected end of file, expecting TC_FALLBACK or '}' in INI on line 1
precision=[14]
from=[]
## the name is all there is
PHP:  syntax error, unexpected end of file, expecting TC_FALLBACK or '}' in INI on line 1
precision=[14]
from=[]
## a newline ends the name
PHP:  syntax error, unexpected end of file, expecting TC_FALLBACK or '}' in INI on line 1
precision=[14]
from=[]
## the fallback never closes
PHP:  syntax error, unexpected end of file, expecting '}' in INI on line 1
precision=[14]
from=[]
## a `;` ends the fallback
PHP:  syntax error, unexpected end of file, expecting '}' in INI on line 1
precision=[14]
from=[]
## a raw quote ends it too
PHP:  syntax error, unexpected end of file, expecting '}' in INI on line 1
precision=[14]
from=[]
## a quote with no partner inside the fallback
PHP:  syntax error, unexpected end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '"' in INI on line 3
precision=[14]
from=[]
## and behind its own text
PHP:  syntax error, unexpected end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '"' in INI on line 3
precision=[14]
from=[]
## a quoted run that closes
precision=[9]
from=[]
## a quoted run counts its newlines
PHP:  syntax error, unexpected end of file, expecting '}' in INI on line 2
precision=[14]
from=[]
## an escaped brace is not the end
precision=[9]
from=[]
## a nested run refuses too
PHP:  syntax error, unexpected '}', expecting TC_VARNAME in INI on line 1
precision=[14]
from=[]
## a nested run that reads
precision=[9]
from=[]
## the second run is screened too
PHP:  syntax error, unexpected '}', expecting TC_VARNAME in INI on line 1
precision=[14]
from=[]
## the run beats the `]` it never reaches
PHP:  syntax error, unexpected end of file, expecting '}' in INI on line 1
precision=[14]
from=[]
## the name goes on around it
precision=[9]
from=[]
## a directive above it stands
PHP:  syntax error, unexpected TC_FALLBACK, expecting TC_VARNAME in INI on line 2
precision=[9]
from=[]
## in an offset name
PHP:  syntax error, unexpected TC_FALLBACK, expecting TC_VARNAME in INI on line 1
precision=[14]
from=[]
## an offset name php reads
precision=[9]
from=[]
## in a value, the same rule
PHP:  syntax error, unexpected TC_FALLBACK, expecting TC_VARNAME in INI on line 1
precision=[14]
from=[]
## a value fallback quote with no partner
PHP:  syntax error, unexpected end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '"' in INI on line 3
precision=[14]
from=[]
## and behind its text
PHP:  syntax error, unexpected end of file, expecting TC_DOLLAR_CURLY or TC_QUOTED_STRING or '"' in INI on line 3
precision=[14]
from=[]
