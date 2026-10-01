--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
display_errors names a destination, and the whole value table says which one
--DESCRIPTION--
The directive is not a boolean. The words on/yes/true/stdout mean program output
and stderr means the error stream, all matched case-insensitively against the
WHOLE value. Everything else is read as a base-ten number the way strtol() reads
one -- leading blanks skipped, an optional sign, trailing garbage ignored -- and
then truncated to eight bits, which is why 256 is off, 257 is stdout and 258 is
stderr. A truncated value that is neither 0, 1 nor 2 means stdout.
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip POSIX-only: shell quoting and 2>/dev/null redirection"; } ?>
--FILE--
<?php
$php = getenv('PHPT_TARGET_EXECUTABLE');
$dir = sys_get_temp_dir();
$src = $dir . '/phl_de_table_' . getmypid() . '.php';
file_put_contents($src, "<?php\n\$u = \$undef;\n");

$values = array(
    'on', 'On', 'yes', 'true', 'stdout', 'stderr', 'StdErr', 'STDOUT',
    'off', 'no', 'false', '',
    '0', '1', '2', '3', '255', '256', '257', '258', '-1', '-2',
    '1abc', '2abc', '0x2', '02', '+2', ' 2',
);
foreach ($values as $v) {
    $cmd = escapeshellarg($php)
         . ' -d ' . escapeshellarg('display_errors=' . $v)
         . ' -d ' . escapeshellarg('log_errors=0')
         . ' ' . escapeshellarg($src);
    // stdout of the child, stderr discarded
    $fp = popen($cmd . ' 2>/dev/null', 'r');
    $out = ''; while (!feof($fp)) { $out .= fgets($fp); } pclose($fp);
    // stderr of the child, stdout discarded
    $fp = popen($cmd . ' 2>&1 1>/dev/null', 'r');
    $err = ''; while (!feof($fp)) { $err .= fgets($fp); } pclose($fp);

    $where = 'off';
    if (strlen(trim($out))) { $where = 'stdout'; }
    if (strlen(trim($err))) { $where = ($where === 'stdout') ? 'BOTH' : 'stderr'; }
    printf("%-8s -> %s\n", "'" . $v . "'", $where);
}
?>
--EXPECT--
'on'     -> stdout
'On'     -> stdout
'yes'    -> stdout
'true'   -> stdout
'stdout' -> stdout
'stderr' -> stderr
'StdErr' -> stderr
'STDOUT' -> stdout
'off'    -> off
'no'     -> off
'false'  -> off
''       -> off
'0'      -> off
'1'      -> stdout
'2'      -> stderr
'3'      -> stdout
'255'    -> stdout
'256'    -> off
'257'    -> stdout
'258'    -> stderr
'-1'     -> stdout
'-2'     -> stdout
'1abc'   -> stdout
'2abc'   -> stderr
'0x2'    -> off
'02'     -> stderr
'+2'     -> stderr
' 2'     -> stderr
--CLEAN--
<?php
@unlink(sys_get_temp_dir() . '/phl_de_table_' . getmypid() . '.php');
