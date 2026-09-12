--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
proc_open(): pipe stdin/stdout/stderr to a child interpreter, redirect stderr, capture the exit code
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip"; } if (!function_exists('proc_open')) { echo "skip proc_open unavailable"; } ?>
--FILE--
<?php
function pox_run($code, $redirect = false) {
    $spec = [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']];
    if ($redirect) { $spec[2] = ['redirect', 1]; }
    $p = proc_open([PHP_BINARY], $spec, $pipes);
    if (!is_resource($p)) { echo "NO-PROC\n"; return; }
    fwrite($pipes[0], $code);
    fclose($pipes[0]);
    $out = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    $err = '';
    if (isset($pipes[2])) { $err = stream_get_contents($pipes[2]); fclose($pipes[2]); }
    $exit = proc_close($p);
    echo "out=[$out] err=[$err] exit=$exit\n";
}
pox_run('<?php fwrite(STDOUT, "hello");');
pox_run('<?php fwrite(STDERR, "oops");');
pox_run('<?php fwrite(STDOUT, "a"); fwrite(STDERR, "b");');
pox_run('<?php fwrite(STDERR, "red");', true);
pox_run('<?php exit(3);');
?>
--EXPECT--
out=[hello] err=[] exit=0
out=[] err=[oops] exit=0
out=[a] err=[b] exit=0
out=[red] err=[] exit=0
out=[] err=[] exit=3
--CLEAN--
<?php
unset($pipes);
