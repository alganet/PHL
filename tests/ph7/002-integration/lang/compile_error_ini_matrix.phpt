--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
display_errors and log_errors gate the MAIN script's own compile diagnostic
--DESCRIPTION--
php reads php.ini before it compiles anything, so a parse error in the file named
on the command line is behind the same two gates a runtime warning is: the log
copy on stderr under log_errors, the display copy on stdout under display_errors,
both when both are on, and NOTHING at all when neither is.

The engine used to reach those gates only through the finished VM -- and
ph7_compile_file is the call that CREATES it, so every diagnostic the main
script's own compile could raise was already out before the first directive
landed. All four combinations printed the one stock-CLI answer: the log copy, on
stderr, always. `-d display_errors=1` moved nothing and `-d log_errors=0`
silenced nothing.

The two copies are not the same bytes: the display copy has no `PHP ` prefix,
one space after the colon, and a leading blank line.

XDEBUG_MODE=off keeps the oracle's own module out of the comparison.
--ENV--
XDEBUG_MODE=off
--FILE--
<?php
$bin  = getenv('PHPT_TARGET_EXECUTABLE');
$dir  = sys_get_temp_dir() . '/phl_ini_matrix_' . getmypid();
@mkdir($dir);
$src  = $dir . '/bad.php';
$errf = $dir . '/err.txt';
file_put_contents($src, "<?php\n\$a = ;\n");

foreach (array(0, 1) as $display) {
    foreach (array(0, 1) as $log) {
        $cmd = '"' . $bin . '"'
             . ' -d display_errors=' . $display
             . ' -d log_errors=' . $log
             . ' "' . $src . '" 2>"' . $errf . '"';
        $fp = popen($cmd, 'r');
        $out = '';
        while (!feof($fp)) {
            $chunk = fgets($fp);
            if ($chunk === false) break;
            $out .= $chunk;
        }
        pclose($fp);
        $err = file_get_contents($errf);
        /* The engine names the file the way the platform spells it (the Windows
         * build answers a backslash path for a temp dir handed over with forward
         * slashes), so the location tail is matched, not the string we built. */
        $out = preg_replace('#in .* on line #', 'in <file> on line ', $out);
        $err = preg_replace('#in .* on line #', 'in <file> on line ', $err);
        printf("display_errors=%d log_errors=%d\n", $display, $log);
        printf("  stdout: %s\n", var_export(trim($out), true));
        printf("  stderr: %s\n", var_export(trim($err), true));
    }
}

@unlink($src);
@unlink($errf);
@rmdir($dir);
?>
--EXPECT--
display_errors=0 log_errors=0
  stdout: ''
  stderr: ''
display_errors=0 log_errors=1
  stdout: ''
  stderr: 'PHP Parse error:  syntax error, unexpected token ";" in <file> on line 2'
display_errors=1 log_errors=0
  stdout: 'Parse error: syntax error, unexpected token ";" in <file> on line 2'
  stderr: ''
display_errors=1 log_errors=1
  stdout: 'Parse error: syntax error, unexpected token ";" in <file> on line 2'
  stderr: 'PHP Parse error:  syntax error, unexpected token ";" in <file> on line 2'
--CLEAN--
<?php
unset($bin, $dir, $src, $errf, $cmd, $fp, $out, $err, $chunk, $display, $log);
