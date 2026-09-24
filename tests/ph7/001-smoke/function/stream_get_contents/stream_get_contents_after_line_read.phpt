--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_get_contents() reads from where the SCRIPT is
--DESCRIPTION--
stream_get_contents() read the DEVICE directly, where every other reader here
goes through the read-ahead the line readers fill. On a file fgets() had already
pulled a whole block out of — which is any file under 8 KB — the device was at
the end, so the everyday "read the header line, then take the rest" idiom
answered "" instead of the rest, and the position it left behind was wrong too.
Its $offset had the same shape one layer over: it seeked the device and kept the
old position's buffered bytes, so the read served them before the new position.
--FILE--
<?php
$sgcShow = function ($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '\n', $out), "\n";
};
$sgcFile = tempnam(sys_get_temp_dir(), 'sgc');
file_put_contents($sgcFile, "line1\nline2\nline3\n");
$sgcOpen = function () use ($sgcFile) { $h = fopen($sgcFile, 'r'); fgets($h); return $h; };

$sgcShow('the rest after fgets', function () use ($sgcOpen) {
    $h = $sgcOpen();
    $s = stream_get_contents($h);
    fclose($h);
    return $s;
});
$sgcShow('bounded read after fgets', function () use ($sgcOpen) {
    $h = $sgcOpen();
    $r = [stream_get_contents($h, 5), ftell($h), fgets($h)];
    fclose($h);
    return $r;
});
$sgcShow('offset rewinds past the buffer', function () use ($sgcOpen) {
    $h = $sgcOpen();
    $r = [stream_get_contents($h, -1, 0), ftell($h)];
    fclose($h);
    return $r;
});
$sgcShow('offset forward', function () use ($sgcFile) {
    $h = fopen($sgcFile, 'r');
    $s = stream_get_contents($h, -1, 6);
    fclose($h);
    return $s;
});
$sgcShow('zero length reads nothing', function () use ($sgcOpen) {
    $h = $sgcOpen();
    $r = [stream_get_contents($h, 0), fgets($h)];
    fclose($h);
    return $r;
});
@unlink($sgcFile);
?>
--EXPECT--
the rest after fgets => 'line2\nline3\n'
bounded read after fgets => array (\n  0 => 'line2',\n  1 => 11,\n  2 => '\n',\n)
offset rewinds past the buffer => array (\n  0 => 'line1\nline2\nline3\n',\n  1 => 18,\n)
offset forward => 'line2\nline3\n'
zero length reads nothing => array (\n  0 => '',\n  1 => 'line2\n',\n)
