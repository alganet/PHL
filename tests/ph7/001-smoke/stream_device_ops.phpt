--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A stream op no device implements answers php's way, not PH7's
--DESCRIPTION--
Every op a device has no handler for used to answer one sentence of its own --
"IO routine(fflush) not implemented in the underlying stream(php) device, PH7 is
returning FALSE" -- and a FALSE. No php message looks like that, and php does not
refuse any of them: a flush with nothing to flush SUCCEEDS, a tell asks the
stream layer's own counter rather than the device, an unsupported seek and an
unsupported truncation each have their own wording, and a stat the device cannot
answer is a silent false. Every symfony/console write ends in fflush($stream).

Two devices had to grow the ops to say it with: php's memory streams answer a
SYNTHETIC stat record (its 0xC device, -1 for the three fields a buffer cannot
have, the buffer's real size, and 0444 rather than 0666 when the MODE STRING held
none of `w`, `a` or `+`), and a directory handle is not a byte stream at all --
its handle is an opendir() pointer, which the file device's read/seek/truncate
were taking for a descriptor NUMBER.
--FILE--
<?php
set_error_handler(function ($sdoNo, $sdoMsg) { echo "  [$sdoNo] $sdoMsg\n"; return true; });

function sdoShow($sdoLabel, $sdoVal) {
    if (is_array($sdoVal)) { $sdoVal = 'array(' . count($sdoVal) . ')'; }
    else { $sdoVal = var_export($sdoVal, true); }
    echo str_pad($sdoLabel, 26), ' => ', $sdoVal, "\n";
}

/* --- a flush with nothing to flush is php's silent TRUE --------------------- */
$sdoMem = fopen('php://memory', 'r+');
sdoShow('memory fflush', fflush($sdoMem));
sdoShow('memory ftell', ftell($sdoMem));
sdoShow('memory fwrite', fwrite($sdoMem, 'abcdef'));
sdoShow('memory ftell', ftell($sdoMem));

/* --- the SYNTHETIC stat record of a memory stream --------------------------- */
$sdoSt = fstat($sdoMem);
sdoShow('memory fstat', $sdoSt);
foreach (['dev', 'ino', 'mode', 'nlink', 'uid', 'gid', 'rdev', 'size',
          'atime', 'mtime', 'ctime', 'blksize', 'blocks'] as $sdoK) {
    echo "  $sdoK=", var_export($sdoSt[$sdoK], true), "\n";
}
sdoShow('memory numeric[7]', $sdoSt[7]);

/* --- php's TEMP_STREAM_READONLY: no `w`, no `a`, no `+` in the mode --------- */
foreach (['r', 'r+', 'w', 'a', 'x', 'c'] as $sdoMode) {
    $sdoH = fopen('php://memory', $sdoMode);
    $sdoS = fstat($sdoH);
    echo str_pad("memory '$sdoMode'", 26), ' => mode=', $sdoS['mode'],
         ' write=', var_export(fwrite($sdoH, 'ab'), true),
         ' trunc=', var_export(ftruncate($sdoH, 4), true), "\n";
    fclose($sdoH);
}

/* --- php://temp is the same device ------------------------------------------ */
$sdoTmp = fopen('php://temp', 'r+');
fwrite($sdoTmp, 'xyz');
$sdoSt = fstat($sdoTmp);
sdoShow('temp fstat size', $sdoSt['size']);
sdoShow('temp fstat mode', $sdoSt['mode']);
fclose($sdoTmp);

/* --- php://output: no seek, no stat, and a flush that still succeeds -------- */
$sdoOut = fopen('php://output', 'w');
sdoShow('output fflush', fflush($sdoOut));
sdoShow('output ftell', ftell($sdoOut));
sdoShow('output fseek', fseek($sdoOut, 0));
sdoShow('output rewind', rewind($sdoOut));
sdoShow('output ftruncate', ftruncate($sdoOut, 0));
sdoShow('output fstat', fstat($sdoOut));
fclose($sdoOut);

/* --- data:// has no WRITER at all, and truncates anyway --------------------- */
$sdoData = fopen('data://text/plain,hello world', 'r');
sdoShow('data fwrite', fwrite($sdoData, 'z'));
sdoShow('data fflush', fflush($sdoData));
$sdoSt = fstat($sdoData);
sdoShow('data fstat mode', $sdoSt['mode']);
sdoShow('data fstat size', $sdoSt['size']);
sdoShow('data ftruncate', ftruncate($sdoData, 5));
sdoShow('data reread', (rewind($sdoData) ? fread($sdoData, 40) : 'no'));
sdoShow('data fstat size', fstat($sdoData)['size']);
fclose($sdoData);
sdoShow('data file_put_contents', file_put_contents('data://text/plain,x', 'q'));
sdoShow('data file_get_contents', file_get_contents('data://text/plain,zz'));

/* --- a directory handle is not a byte stream -------------------------------- */
$sdoDir = sys_get_temp_dir() . '/phl_sdo_' . getmypid();
@mkdir($sdoDir);
file_put_contents("$sdoDir/one", 'A');
file_put_contents("$sdoDir/two", 'BB');
$sdoD = opendir($sdoDir);
sdoShow('dir fflush', fflush($sdoD));
sdoShow('dir fstat', fstat($sdoD));
sdoShow('dir fread', fread($sdoD, 4));
sdoShow('dir fgets', fgets($sdoD));
sdoShow('dir fgetc', fgetc($sdoD));
sdoShow('dir feof', feof($sdoD));
sdoShow('dir fpassthru', fpassthru($sdoD));
sdoShow('dir fscanf', fscanf($sdoD, '%s'));
sdoShow('dir stream_get_line', stream_get_line($sdoD, 10, "\n"));
sdoShow('dir fwrite', fwrite($sdoD, 'xyz'));
sdoShow('dir fprintf', fprintf($sdoD, '%s', 'q'));
sdoShow('dir ftruncate', ftruncate($sdoD, 0));
/* php formats the WHOLE string and writes it once, so the length it answers is
 * the same whether the stream took the bytes or refused every one -- and the
 * argument screens come first, with no notice of their own. */
sdoShow('dir fprintf chunks', fprintf($sdoD, 'a%sb%s', 'q', 'ZZ'));
sdoShow('dir vfprintf chunks', vfprintf($sdoD, 'a%sb%s', ['q', 'ZZ']));
sdoShow('dir fprintf empty', fprintf($sdoD, ''));
try { fprintf($sdoD, '%q', 'x'); } catch (\Throwable $sdoE) {
    sdoShow('dir fprintf bad spec', get_class($sdoE) . ': ' . $sdoE->getMessage());
}
/* The position a directory handle reports is php's own record counter: it starts
 * at 0, steps by ONE record per entry produced, the read that finds the end moves
 * nothing, and a seek REWINDS the directory without putting the counter back. The
 * record size is php's MAXPATHLEN + 1 and differs per platform, so what is pinned
 * here is the shape. */
$sdoP0 = ftell($sdoD);
$sdoNames = [];
while (false !== ($sdoE = readdir($sdoD))) { $sdoNames[] = $sdoE; }
$sdoP1 = ftell($sdoD);
sort($sdoNames);
sdoShow('dir entries', json_encode($sdoNames));
sdoShow('dir start', $sdoP0);
sdoShow('dir step is uniform', $sdoP1 === $sdoP0 + count($sdoNames) * (int) ($sdoP1 / count($sdoNames)));
sdoShow('dir fseek', fseek($sdoD, 0));
sdoShow('dir tell after seek', ftell($sdoD) === $sdoP1);
sdoShow('dir rewind', rewind($sdoD));
sdoShow('dir reread', readdir($sdoD) !== false);
closedir($sdoD);
@unlink("$sdoDir/one");
@unlink("$sdoDir/two");
@rmdir($sdoDir);

/* --- a wrapper with no directory opener ------------------------------------- */
sdoShow('opendir php://memory', opendir('php://memory'));
sdoShow('opendir data://', opendir('data://text/plain,x'));
fclose($sdoMem);
restore_error_handler();
--EXPECT--
memory fflush              => true
memory ftell               => 0
memory fwrite              => 6
memory ftell               => 6
memory fstat               => array(26)
  dev=12
  ino=0
  mode=33206
  nlink=1
  uid=0
  gid=0
  rdev=-1
  size=6
  atime=0
  mtime=0
  ctime=0
  blksize=-1
  blocks=-1
memory numeric[7]          => 6
memory 'r'                 => mode=33060 write=false trunc=false
memory 'r+'                => mode=33206 write=2 trunc=true
memory 'w'                 => mode=33206 write=2 trunc=true
memory 'a'                 => mode=33206 write=2 trunc=true
memory 'x'                 => mode=33060 write=false trunc=false
memory 'c'                 => mode=33060 write=false trunc=false
temp fstat size            => 3
temp fstat mode            => 33206
output fflush              => true
output ftell               => 0
  [2] fseek(): Stream does not support seeking
output fseek               => -1
  [2] rewind(): Stream does not support seeking
output rewind              => false
  [2] ftruncate(): Can't truncate this stream!
output ftruncate           => false
output fstat               => false
  [8] fwrite(): Stream is not writable
data fwrite                => false
data fflush                => true
data fstat mode            => 33206
data fstat size            => 11
data ftruncate             => true
data reread                => 'hello'
data fstat size            => 5
  [8] file_put_contents(): Stream is not writable
data file_put_contents     => false
data file_get_contents     => 'zz'
dir fflush                 => true
dir fstat                  => false
dir fread                  => false
dir fgets                  => false
dir fgetc                  => false
dir feof                   => false
dir fpassthru              => -1
dir fscanf                 => false
dir stream_get_line        => false
  [8] fwrite(): Stream is not writable
dir fwrite                 => false
  [8] fprintf(): Stream is not writable
dir fprintf                => 1
  [2] ftruncate(): Can't truncate this stream!
dir ftruncate              => false
  [8] fprintf(): Stream is not writable
dir fprintf chunks         => 5
  [8] vfprintf(): Stream is not writable
dir vfprintf chunks        => 5
dir fprintf empty          => 0
dir fprintf bad spec       => 'ValueError: Unknown format specifier "q"'
dir entries                => '[".","..","one","two"]'
dir start                  => 0
dir step is uniform        => true
dir fseek                  => 0
dir tell after seek        => true
dir rewind                 => true
dir reread                 => true
  [2] opendir(php://memory): Failed to open directory: not implemented
opendir php://memory       => false
  [2] opendir(data://text/plain,x): Failed to open directory: not implemented
opendir data://            => false
