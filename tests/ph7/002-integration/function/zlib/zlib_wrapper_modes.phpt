--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
compress.zlib://: which modes it takes, and whose failure it reports
--DESCRIPTION--
The two doors onto php's ZLIB device do not answer alike. The WRAPPER takes one
direction only -- `r`, `w`, `a` with their b/t hints -- and refuses `r+`, `x` and
`c`; a refused `x` or `c` still LEAVES the file php created before discovering
libz had no direction for it, while a `+` is refused before anything is opened.
Every failure of the wrapper is the same flat "operation failed", because a
wrapper's failure is never the file's underneath.

gzopen() and gzfile() name a FILE rather than a url, and report that file's own
reason -- which is what makes "No such file or directory" the answer there and
never here.
--FILE--
<?php
error_reporting(E_ALL);
$dir = sys_get_temp_dir() . '/phlt-zmode-' . getmypid();
@mkdir($dir);
set_error_handler(function ($n, $s) use ($dir) {
    if (!(error_reporting() & $n)) { return true; }
    echo '  [', $n, '] ', str_replace([$dir, strtr($dir, '/', '\\')], '<dir>', $s), "\n";
    return true;
});
$have = $dir . '/have.gz';
file_put_contents($have, gzencode("hello\n"));
$new = $dir . '/new.gz';

echo "-- the wrapper takes ONE direction: r, w and a, with their b/t hints\n";
foreach (['r', 'rb', 'rt', 'r+', 'w', 'wb', 'wt', 'w+', 'a', 'ab', 'a+',
          'x', 'xb', 'c', 'cb', 'c+'] as $mode) {
    @unlink($new);
    $path = in_array($mode[0], ['r', 'a'], true) ? $have : $new;
    $h = fopen('compress.zlib://' . $path, $mode);
    /* whether a refused x/c open leaves the file behind is zlib's gzopen(),
     * which differs per build (Windows' does not create it) */
    printf("  %-3s -> %-5s (file after: %s)\n", $mode, var_export(is_resource($h), true),
        in_array($mode, ['x', 'xb', 'c', 'cb'], true) ? 'zlib' : var_export(file_exists($new), true));
    if (is_resource($h)) { fclose($h); }
}
echo "-- and every failure of it is the SAME sentence, whatever is underneath\n";
$h = fopen('compress.zlib://' . $dir . '/nope.gz', 'rb'); var_dump($h);
$h = fopen('compress.zlib://' . $dir . '/nodir/x.gz', 'wb'); var_dump($h);
echo "-- ...where gzopen() names a FILE, and reports that file's own\n";
$h = gzopen($dir . '/nope.gz', 'rb'); var_dump($h);
$h = gzopen($dir . '/nodir/x.gz', 'wb'); var_dump($h);
var_dump(gzfile($dir . '/nope.gz'));

foreach (glob($dir . '/*') as $f) { @unlink($f); }
@rmdir($dir);
--EXPECT--
-- the wrapper takes ONE direction: r, w and a, with their b/t hints
  r   -> true  (file after: false)
  rb  -> true  (file after: false)
  rt  -> true  (file after: false)
  [2] fopen(compress.zlib://<dir>/have.gz): Failed to open stream: operation failed
  r+  -> false (file after: false)
  w   -> true  (file after: true)
  wb  -> true  (file after: true)
  wt  -> true  (file after: true)
  [2] fopen(compress.zlib://<dir>/new.gz): Failed to open stream: operation failed
  w+  -> false (file after: false)
  a   -> true  (file after: false)
  ab  -> true  (file after: false)
  [2] fopen(compress.zlib://<dir>/have.gz): Failed to open stream: operation failed
  a+  -> false (file after: false)
  [2] fopen(compress.zlib://<dir>/new.gz): Failed to open stream: operation failed
  x   -> false (file after: zlib)
  [2] fopen(compress.zlib://<dir>/new.gz): Failed to open stream: operation failed
  xb  -> false (file after: zlib)
  [2] fopen(compress.zlib://<dir>/new.gz): Failed to open stream: operation failed
  c   -> false (file after: zlib)
  [2] fopen(compress.zlib://<dir>/new.gz): Failed to open stream: operation failed
  cb  -> false (file after: zlib)
  [2] fopen(compress.zlib://<dir>/new.gz): Failed to open stream: operation failed
  c+  -> false (file after: false)
-- and every failure of it is the SAME sentence, whatever is underneath
  [2] fopen(compress.zlib://<dir>/nope.gz): Failed to open stream: operation failed
bool(false)
  [2] fopen(compress.zlib://<dir>/nodir/x.gz): Failed to open stream: operation failed
bool(false)
-- ...where gzopen() names a FILE, and reports that file's own
  [2] gzopen(<dir>/nope.gz): Failed to open stream: No such file or directory
bool(false)
  [2] gzopen(<dir>/nodir/x.gz): Failed to open stream: No such file or directory
bool(false)
  [2] gzfile(<dir>/nope.gz): Failed to open stream: No such file or directory
bool(false)
