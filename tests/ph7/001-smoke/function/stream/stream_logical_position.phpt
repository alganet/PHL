--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a write lands where the script is, and ftruncate() does not move it
--DESCRIPTION--
The readers here pull AHEAD of the script — a line reader fills 8 KB, and a read
filter's output waits in a buffer of its own — so every write has to step back
over both before it lands. fwrite(), fputcsv() and stream_copy_to_stream() each
stepped back over the line buffer only, so a write on a filtered handle landed
at the device's offset (an append) instead of at the script's. And ftruncate()
DISCARDED what was buffered, which php does not do: truncating is not a seek,
so ftell() must not move and the next read must still answer what was already
pulled ahead.
--FILE--
<?php
$slpShow = function ($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '\n', $out), "\n";
};
$slpFile = tempnam(sys_get_temp_dir(), 'slp');
$slpFill = function ($s) use ($slpFile) { file_put_contents($slpFile, $s); return $slpFile; };

/* A filtered read leaves the device far past the script; the write goes where
 * the script is. rot13 is 1:1, so the two positions are comparable. */
$slpShow('fwrite after a filtered read', function () use ($slpFill, $slpFile) {
    $h = fopen($slpFill(str_repeat('a', 40)), 'r+');
    stream_filter_append($h, 'string.rot13');
    fread($h, 10);
    fwrite($h, 'ZZ');
    fclose($h);
    return [filesize($slpFile), substr(file_get_contents($slpFile), 8, 6)];
});
$slpShow('fwrite after a line read', function () use ($slpFill, $slpFile) {
    $h = fopen($slpFill("aaa\nbbb\nccc\n"), 'r+');
    fgets($h);
    fwrite($h, 'ZZ');
    fclose($h);
    return file_get_contents($slpFile);
});
$slpShow('fputcsv after a line read', function () use ($slpFill, $slpFile) {
    $h = fopen($slpFill("aaa\nbbbbbbbb\n"), 'r+');
    fgets($h);
    fputcsv($h, ['x', 'y'], ',', '"', '');
    fclose($h);
    return file_get_contents($slpFile);
});
$slpShow('copy_to_stream after a line read', function () use ($slpFill, $slpFile) {
    $h = fopen($slpFill("aaa\nbbbbbb\n"), 'r+');
    fgets($h);
    $src = fopen('php://memory', 'w+');
    fwrite($src, 'XY');
    rewind($src);
    stream_copy_to_stream($src, $h);
    fclose($src);
    fclose($h);
    return file_get_contents($slpFile);
});

/* ftruncate() leaves the position and the buffer alone. */
$slpShow('truncate below the buffer', function () use ($slpFill) {
    $h = fopen($slpFill("aaa\nbbb\nccc\nddd\n"), 'r+');
    fgets($h);
    ftruncate($h, 6);
    $r = [ftell($h), fgets($h), feof($h)];
    fclose($h);
    return $r;
});
$slpShow('truncate above the file', function () use ($slpFill) {
    $h = fopen($slpFill("aaa\nbbb\nccc\nddd\n"), 'r+');
    fgets($h);
    ftruncate($h, 100);
    $r = [ftell($h), fgets($h)];
    fclose($h);
    return $r;
});

@unlink($slpFile);
?>
--EXPECT--
fwrite after a filtered read => array (\n  0 => 40,\n  1 => 'aaMMaa',\n)
fwrite after a line read => 'aaa\nZZb\nccc\n'
fputcsv after a line read => 'aaa\nx,y\nbbbb\n'
copy_to_stream after a line read => 'aaa\nXYbbbb\n'
truncate below the buffer => array (\n  0 => 4,\n  1 => 'bbb\n',\n  2 => false,\n)
truncate above the file => array (\n  0 => 4,\n  1 => 'bbb\n',\n)
