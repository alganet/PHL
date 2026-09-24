--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: feof() reports a read that already came back empty, and never reads to answer
--FILE--
<?php
/* php's end-of-file flag is set AFTER THE FACT, and asking never reads. PHL
 * used to probe the device from inside feof(), which said TRUE about a handle
 * nothing had read, said TRUE about a WRITE-only handle whose refused read
 * looked like an end, and BLOCKED on feof(STDIN) with no input waiting. */
$eofm_path = sys_get_temp_dir() . '/phl_eofmodel_' . getmypid() . '.txt';
file_put_contents($eofm_path, "hello\nworld\n");

$h = fopen($eofm_path, 'r');
fgets($h);
fgets($h);
echo 'after every line: ', var_export(feof($h), true), "\n";
fclose($h);

/* A read for MORE than is there finds the end; one for exactly what is left
 * does not. */
$h = fopen($eofm_path, 'r');
fread($h, 100);
echo 'after over-read: ', var_export(feof($h), true), "\n";
fclose($h);

$h = fopen($eofm_path, 'r');
fread($h, 12);
echo 'after exact read: ', var_export(feof($h), true), "\n";
/* and a seek takes the flag back off */
fread($h, 100);
rewind($h);
echo 'after rewind: ', var_export(feof($h), true), "\n";
fclose($h);

$empty = sys_get_temp_dir() . '/phl_eofmodel_empty_' . getmypid() . '.txt';
file_put_contents($empty, '');
$h = fopen($empty, 'r');
echo 'empty file, unread: ', var_export(feof($h), true), "\n";
fclose($h);

$m = fopen('php://memory', 'r+');
echo 'fresh memory: ', var_export(stream_get_meta_data($m)['eof'], true), "\n";
fclose($m);

/* A write-only handle is not at EOF: a read ERROR is not an end of file. */
$w = fopen($eofm_path, 'w');
echo 'write-only: ', var_export(feof($w), true), "\n";
fclose($w);

/* A userland wrapper answers for itself — php calls stream_eof(). */
class EofmWrapper
{
    public $context;
    private $left = 2;
    public function stream_open($path, $mode, $options, &$opened_path) { return true; }
    public function stream_read($count) { return $this->left-- > 0 ? 'ab' : ''; }
    public function stream_eof() { return $this->left <= 0; }
    public function stream_stat() { return []; }
}
stream_wrapper_register('eofm', 'EofmWrapper');
$u = fopen('eofm://x', 'r');
echo 'userland unread: ', var_export(feof($u), true), "\n";
fread($u, 2);
fread($u, 2);
echo 'userland drained: ', var_export(feof($u), true), "\n";
fclose($u);

/* The loop everyone writes still terminates on exactly php's iteration count
 * — over a file the write-only open above has just truncated, and then over a
 * two-line one. */
$h = fopen($eofm_path, 'r');
$n = 0;
while (!feof($h)) { $line = fgets($h); $n++; }
echo 'empty fgets loop iterations: ', $n, "\n";
fclose($h);

file_put_contents($eofm_path, "hello\nworld\n");
$h = fopen($eofm_path, 'r');
$n = 0;
while (!feof($h)) { $line = fgets($h); $n++; }
echo 'fgets loop iterations: ', $n, "\n";
fclose($h);

unlink($eofm_path);
unlink($empty);
?>
--EXPECT--
after every line: false
after over-read: true
after exact read: false
after rewind: false
empty file, unread: false
fresh memory: false
write-only: false
userland unread: false
userland drained: true
empty fgets loop iterations: 1
fgets loop iterations: 3
