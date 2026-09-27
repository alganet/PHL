--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Only fopen()'s first mode character decides, and a failed read or write says so
--DESCRIPTION--
php's php_stream_parse_fopen_modes is narrower than the manual reads: the FIRST
character decides and must be one of `r w a x c` in lower case, everything after it
is merely SCANNED for `+` (read-write) and `b`/`t` (translation), and anything else
is refused outright with a warning. This engine read only the first TWO characters
and had its own idea of both halves, so six ordinary spellings opened the wrong way
in silence -- `rb+`, `ab+` and `cb+` were opened read-only or write-only because the
`+` is not in position two, `rw` and `wr` were read-WRITE where php gives one
direction, and an unknown or upper-case mode was accepted with a PH7-ism notice and
a read-only handle where php refuses the call. Beside them the two notices php's
plain-file stream ops raise when the device refuses: a read or a write on a handle
opened the other way answers false in both engines, and only php said why.
--FILE--
<?php
$fmDir = sys_get_temp_dir() . '/phl_fopenmode_' . getmypid();
@mkdir($fmDir);
$fmPath = "$fmDir/m.txt";
/* The errno NUMBER and its text are the platform's, and the scratch path is this
 * run's; the shape of each is php's. */
$GLOBALS['fmDirName'] = $fmDir;
function fmNorm($s) {
    return preg_replace('/errno=\d+ .*$/', 'errno=<N> <text>',
        str_replace($GLOBALS['fmDirName'], '<dir>', $s));
}
set_error_handler(function ($no, $str) { echo '  E(', $no, '): ', fmNorm($str), "\n"; return true; });

/* What each mode OPENS: the direction it grants, and the file it leaves. */
foreach (['r', 'rb', 'rt', 'r+', 'rb+', 'r+b', 'rw', 'w', 'wr', 'wb', 'w+', 'wb+',
          'a', 'a+', 'ab+', 'c', 'c+', 'cb+', 'x', 'x+', 'rbt', 'rtb', 'r++',
          'R', 'W', 'A', 'X', 'C', 'B', 'T', '', 'b', 't', '+', '+r', 'br', 'zz',
          ' r', 'e', 'n', 'rn', 'r-', 'abc'] as $fmMode) {
    file_put_contents($fmPath, "hello\n");
    echo str_pad(var_export($fmMode, true), 7), ' => ';
    $h = fopen($fmPath, $fmMode);
    if ($h === false) { echo "false\n"; continue; }
    $w = fwrite($h, 'Z');
    fseek($h, 0);
    $r = fread($h, 8);
    fclose($h);
    echo 'write=', var_export($w, true), ' read=', json_encode($r),
         ' file=', json_encode(file_get_contents($fmPath)), "\n";
}
/* The refusal names the mode it was handed, whatever the mode is. */
echo "-- the refusal\n";
$h = fopen($fmPath, 'zz');
var_dump($h);

/* The two notices are the DEVICE's: every reader and every writer raises them,
 * and the read reports the CHUNK size rather than the length asked for. */
echo "-- a read on a write-only handle\n";
foreach (['fread', 'fgets', 'fgetc', 'stream_get_contents', 'fpassthru'] as $fmFn) {
    $h = fopen($fmPath, 'w');
    echo str_pad($fmFn, 20), ' => ';
    $out = $fmFn === 'fread' ? $fmFn($h, 4) : $fmFn($h);
    var_dump($out);
    fclose($h);
}
echo "-- and the chunk size is what it names\n";
$h = fopen($fmPath, 'w');
stream_set_chunk_size($h, 100);
var_dump(fread($h, 4));
fclose($h);

echo "-- a write on a read-only handle\n";
$h = fopen($fmPath, 'r');
var_dump(fwrite($h, 'Z'));
var_dump(fputcsv($h, ['a'], ',', '"', '\\'));
var_dump(fprintf($h, '%s', 'a'));
/* php's fflush() over a read-only stream is a no-op that SUCCEEDS; on Windows the
 * flush the engine makes is refused at the handle and has to answer the same. */
var_dump(fflush($h));
fclose($h);

restore_error_handler();
@unlink($fmPath); @rmdir($fmDir);
--EXPECT--
'r'     =>   E(8): fwrite(): Write of 1 bytes failed with errno=<N> <text>
write=false read="hello\n" file="hello\n"
'rb'    =>   E(8): fwrite(): Write of 1 bytes failed with errno=<N> <text>
write=false read="hello\n" file="hello\n"
'rt'    =>   E(8): fwrite(): Write of 1 bytes failed with errno=<N> <text>
write=false read="hello\n" file="hello\n"
'r+'    => write=1 read="Zello\n" file="Zello\n"
'rb+'   => write=1 read="Zello\n" file="Zello\n"
'r+b'   => write=1 read="Zello\n" file="Zello\n"
'rw'    =>   E(8): fwrite(): Write of 1 bytes failed with errno=<N> <text>
write=false read="hello\n" file="hello\n"
'w'     =>   E(8): fread(): Read of 8192 bytes failed with errno=<N> <text>
write=1 read=false file="Z"
'wr'    =>   E(8): fread(): Read of 8192 bytes failed with errno=<N> <text>
write=1 read=false file="Z"
'wb'    =>   E(8): fread(): Read of 8192 bytes failed with errno=<N> <text>
write=1 read=false file="Z"
'w+'    => write=1 read="Z" file="Z"
'wb+'   => write=1 read="Z" file="Z"
'a'     =>   E(8): fread(): Read of 8192 bytes failed with errno=<N> <text>
write=1 read=false file="hello\nZ"
'a+'    => write=1 read="hello\nZ" file="hello\nZ"
'ab+'   => write=1 read="hello\nZ" file="hello\nZ"
'c'     =>   E(8): fread(): Read of 8192 bytes failed with errno=<N> <text>
write=1 read=false file="Zello\n"
'c+'    => write=1 read="Zello\n" file="Zello\n"
'cb+'   => write=1 read="Zello\n" file="Zello\n"
'x'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: File exists
false
'x+'    =>   E(2): fopen(<dir>/m.txt): Failed to open stream: File exists
false
'rbt'   =>   E(8): fwrite(): Write of 1 bytes failed with errno=<N> <text>
write=false read="hello\n" file="hello\n"
'rtb'   =>   E(8): fwrite(): Write of 1 bytes failed with errno=<N> <text>
write=false read="hello\n" file="hello\n"
'r++'   => write=1 read="Zello\n" file="Zello\n"
'R'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `R' is not a valid mode for fopen
false
'W'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `W' is not a valid mode for fopen
false
'A'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `A' is not a valid mode for fopen
false
'X'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `X' is not a valid mode for fopen
false
'C'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `C' is not a valid mode for fopen
false
'B'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `B' is not a valid mode for fopen
false
'T'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `T' is not a valid mode for fopen
false
''      =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `' is not a valid mode for fopen
false
'b'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `b' is not a valid mode for fopen
false
't'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `t' is not a valid mode for fopen
false
'+'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `+' is not a valid mode for fopen
false
'+r'    =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `+r' is not a valid mode for fopen
false
'br'    =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `br' is not a valid mode for fopen
false
'zz'    =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `zz' is not a valid mode for fopen
false
' r'    =>   E(2): fopen(<dir>/m.txt): Failed to open stream: ` r' is not a valid mode for fopen
false
'e'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `e' is not a valid mode for fopen
false
'n'     =>   E(2): fopen(<dir>/m.txt): Failed to open stream: `n' is not a valid mode for fopen
false
'rn'    =>   E(8): fwrite(): Write of 1 bytes failed with errno=<N> <text>
write=false read="hello\n" file="hello\n"
'r-'    =>   E(8): fwrite(): Write of 1 bytes failed with errno=<N> <text>
write=false read="hello\n" file="hello\n"
'abc'   =>   E(8): fread(): Read of 8192 bytes failed with errno=<N> <text>
write=1 read=false file="hello\nZ"
-- the refusal
  E(2): fopen(<dir>/m.txt): Failed to open stream: `zz' is not a valid mode for fopen
bool(false)
-- a read on a write-only handle
fread                =>   E(8): fread(): Read of 8192 bytes failed with errno=<N> <text>
bool(false)
fgets                =>   E(8): fgets(): Read of 8192 bytes failed with errno=<N> <text>
bool(false)
fgetc                =>   E(8): fgetc(): Read of 8192 bytes failed with errno=<N> <text>
bool(false)
stream_get_contents  =>   E(8): stream_get_contents(): Read of 8192 bytes failed with errno=<N> <text>
string(0) ""
fpassthru            =>   E(8): fpassthru(): Read of 8192 bytes failed with errno=<N> <text>
int(-1)
-- and the chunk size is what it names
  E(8): fread(): Read of 100 bytes failed with errno=<N> <text>
bool(false)
-- a write on a read-only handle
  E(8): fwrite(): Write of 1 bytes failed with errno=<N> <text>
bool(false)
  E(8): fputcsv(): Write of 2 bytes failed with errno=<N> <text>
bool(false)
  E(8): fprintf(): Write of 1 bytes failed with errno=<N> <text>
int(1)
bool(true)
