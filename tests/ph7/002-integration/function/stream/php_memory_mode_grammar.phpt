--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php://memory and php://temp read the whole mode string, not the usual mode grammar
--FILE--
<?php
/* The two memory devices do not open the way every other stream does. php's
 * wrapper asks ONE question of the mode -- `strpbrk(mode, "wa+")` -- so:
 *
 *  - a `w`, an `a` or a `+` ANYWHERE in the string builds a writable buffer.
 *    Position is not part of it, so `"rw"`, `"+r"` and `"bw"` all write, and
 *    the ordinary first-character grammar has no say at all.
 *  - nothing else does. `"r"`, `"rb"`, `"rt"` and even `"x"` and `"c"` -- which
 *    are write modes on any other device -- open read-only here, and a write to
 *    one is a silent false.
 *  - the test is case-SENSITIVE: `"W"` and `"A"` open read-only.
 *  - the mode the stream then REPORTS is decided from the same string: any
 *    lowercase `a` makes it `a+b` whatever else is there, any other writable
 *    spelling `w+b`, and a read-only one `rb`. So `"ra"` reports `a+b` and
 *    `"wa"` does too.
 *
 * `"rw"` is not a curiosity: it is what Composer's BufferIO opens, and reading
 * the parsed FLAG bits instead of the string made every line it captured come
 * back empty.
 */
$modes = ['r','rb','r+','rw','wr','ra','r+b','w','wb','w+','a','a+','x','x+',
          'c','c+','rt','rwb','ar','+','+r','br','bw','R','W','A','rW','',
          'z','rz','r-','r+w','wa','aw','+a','w+a','ba','ab','xa','ca','Ab'];
foreach (['php://memory', 'php://temp'] as $url) {
    echo "== $url\n";
    foreach ($modes as $m) {
        $h = @fopen($url, $m);
        if ($h === false) {
            echo str_pad(var_export($m, true), 7), " fopen=FALSE\n";
            continue;
        }
        $n  = @fwrite($h, 'abc');
        @fseek($h, 0);
        $got = @stream_get_contents($h);
        $md  = @stream_get_meta_data($h);
        echo str_pad(var_export($m, true), 7),
             ' write=', str_pad(var_export($n, true), 5),
             ' read=',  str_pad(var_export($got, true), 7),
             ' mode=',  var_export($md['mode'], true), "\n";
        fclose($h);
    }
}
/* The read-only buffer refuses truncation the same way, and stats 0100444
 * where a writable one stats 0100666. */
echo "== stat\n";
foreach (['r', 'rw'] as $m) {
    $h = fopen('php://memory', $m);
    $st = fstat($h);
    echo str_pad(var_export($m, true), 7), ' mode=', decoct($st['mode']),
         ' ftruncate=', var_export(@ftruncate($h, 0), true), "\n";
    fclose($h);
}
--EXPECT--
== php://memory
'r'     write=false read=''      mode='rb'
'rb'    write=false read=''      mode='rb'
'r+'    write=3     read='abc'   mode='w+b'
'rw'    write=3     read='abc'   mode='w+b'
'wr'    write=3     read='abc'   mode='w+b'
'ra'    write=3     read='abc'   mode='a+b'
'r+b'   write=3     read='abc'   mode='w+b'
'w'     write=3     read='abc'   mode='w+b'
'wb'    write=3     read='abc'   mode='w+b'
'w+'    write=3     read='abc'   mode='w+b'
'a'     write=3     read='abc'   mode='a+b'
'a+'    write=3     read='abc'   mode='a+b'
'x'     write=false read=''      mode='rb'
'x+'    write=3     read='abc'   mode='w+b'
'c'     write=false read=''      mode='rb'
'c+'    write=3     read='abc'   mode='w+b'
'rt'    write=false read=''      mode='rb'
'rwb'   write=3     read='abc'   mode='w+b'
'ar'    write=3     read='abc'   mode='a+b'
'+'     write=3     read='abc'   mode='w+b'
'+r'    write=3     read='abc'   mode='w+b'
'br'    write=false read=''      mode='rb'
'bw'    write=3     read='abc'   mode='w+b'
'R'     write=false read=''      mode='rb'
'W'     write=false read=''      mode='rb'
'A'     write=false read=''      mode='rb'
'rW'    write=false read=''      mode='rb'
''      write=false read=''      mode='rb'
'z'     write=false read=''      mode='rb'
'rz'    write=false read=''      mode='rb'
'r-'    write=false read=''      mode='rb'
'r+w'   write=3     read='abc'   mode='w+b'
'wa'    write=3     read='abc'   mode='a+b'
'aw'    write=3     read='abc'   mode='a+b'
'+a'    write=3     read='abc'   mode='a+b'
'w+a'   write=3     read='abc'   mode='a+b'
'ba'    write=3     read='abc'   mode='a+b'
'ab'    write=3     read='abc'   mode='a+b'
'xa'    write=3     read='abc'   mode='a+b'
'ca'    write=3     read='abc'   mode='a+b'
'Ab'    write=false read=''      mode='rb'
== php://temp
'r'     write=false read=''      mode='rb'
'rb'    write=false read=''      mode='rb'
'r+'    write=3     read='abc'   mode='w+b'
'rw'    write=3     read='abc'   mode='w+b'
'wr'    write=3     read='abc'   mode='w+b'
'ra'    write=3     read='abc'   mode='a+b'
'r+b'   write=3     read='abc'   mode='w+b'
'w'     write=3     read='abc'   mode='w+b'
'wb'    write=3     read='abc'   mode='w+b'
'w+'    write=3     read='abc'   mode='w+b'
'a'     write=3     read='abc'   mode='a+b'
'a+'    write=3     read='abc'   mode='a+b'
'x'     write=false read=''      mode='rb'
'x+'    write=3     read='abc'   mode='w+b'
'c'     write=false read=''      mode='rb'
'c+'    write=3     read='abc'   mode='w+b'
'rt'    write=false read=''      mode='rb'
'rwb'   write=3     read='abc'   mode='w+b'
'ar'    write=3     read='abc'   mode='a+b'
'+'     write=3     read='abc'   mode='w+b'
'+r'    write=3     read='abc'   mode='w+b'
'br'    write=false read=''      mode='rb'
'bw'    write=3     read='abc'   mode='w+b'
'R'     write=false read=''      mode='rb'
'W'     write=false read=''      mode='rb'
'A'     write=false read=''      mode='rb'
'rW'    write=false read=''      mode='rb'
''      write=false read=''      mode='rb'
'z'     write=false read=''      mode='rb'
'rz'    write=false read=''      mode='rb'
'r-'    write=false read=''      mode='rb'
'r+w'   write=3     read='abc'   mode='w+b'
'wa'    write=3     read='abc'   mode='a+b'
'aw'    write=3     read='abc'   mode='a+b'
'+a'    write=3     read='abc'   mode='a+b'
'w+a'   write=3     read='abc'   mode='a+b'
'ba'    write=3     read='abc'   mode='a+b'
'ab'    write=3     read='abc'   mode='a+b'
'xa'    write=3     read='abc'   mode='a+b'
'ca'    write=3     read='abc'   mode='a+b'
'Ab'    write=false read=''      mode='rb'
== stat
'r'     mode=100444 ftruncate=false
'rw'    mode=100666 ftruncate=true
