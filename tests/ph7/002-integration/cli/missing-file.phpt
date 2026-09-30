--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A file the CLI cannot open is php's message, on php's stream, with php's code
--DESCRIPTION--
php names the file it could not open -- `Could not open input file: <path>` -- says
so on STDERR, and exits 1 for both `-l` and a run. PHL answered a generic "IO error
while opening the target file" on STDOUT for a run, and exited 255 rather than 1
from `-l`. A file that does not PARSE is the other code: 255, for both engines.
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip POSIX-only: separates stdout from stderr with a shell redirection"; } ?>
--FILE--
<?php
$phl     = getenv('PHPT_TARGET_EXECUTABLE');
$missing = sys_get_temp_dir() . '/phl_no_such_file_' . getmypid() . '.php';
@unlink($missing);

function mfRun($cmd) {
    $fp = popen($cmd, 'r');
    $o = ''; while (!feof($fp)) { $o .= fgets($fp); }
    return array($o, pclose($fp));
}

foreach (array('-l ', '') as $mode) {
    list($out, $rc) = mfRun("\"$phl\" $mode\"$missing\" 2>/dev/null");
    var_dump($out);
    list($err, $rc) = mfRun("\"$phl\" $mode\"$missing\" 2>&1 1>/dev/null");
    echo str_replace($missing, 'MISSING', $err);
    echo "exit=$rc\n";
}

$bad = sys_get_temp_dir() . '/phl_unparsable_' . getmypid() . '.php';
file_put_contents($bad, "<?php \$x = ;\n");
list($out, $rc) = mfRun("\"$phl\" -l \"$bad\" 2>/dev/null");
echo "parse_exit=$rc\n";
@unlink($bad);
?>
--EXPECT--
string(0) ""
Could not open input file: MISSING
exit=1
string(0) ""
Could not open input file: MISSING
exit=1
parse_exit=255
