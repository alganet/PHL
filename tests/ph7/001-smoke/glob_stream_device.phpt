--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A glob:// URI opens a DIRECTORY whose entries are the pattern's matches
--DESCRIPTION--
php's glob wrapper is a dir_opener and nothing else, and this engine had no such
device at all -- `opendir('glob://*.txt')` was `Unable to find the wrapper "glob"`,
and every directory object's `glob` debug key was therefore always false. The
entries are BASENAMES and their set and ORDER are glob()'s own, which the table at
the bottom pins pattern by pattern so the C walk behind the device and the prelude
glob() cannot drift apart. A pattern with a trailing slash is where they part, and
php parts there too: the match is `d/`, whose part after the last slash is EMPTY,
and an empty entry is what a directory walk reads as the end.
--FILE--
<?php
/* php's glob:// wrapper is a DIRECTORY whose entries are a pattern's matches.
 * It has a dir_opener and nothing else: fopen() on one is a refusal naming the
 * wrapper, and stat() has nothing to answer. The entries are BASENAMES, and the
 * set and the ORDER of them are glob()'s own -- which is what the table at the
 * bottom pins, so that the two implementations cannot drift apart. */
$gsdDir = sys_get_temp_dir() . '/phl_gsd_' . getmypid();
@mkdir($gsdDir);
foreach (['s1', 's2', 'a-b'] as $gsdD) { @mkdir("$gsdDir/$gsdD"); }
foreach (['1.txt', '2.txt', '10.txt', '9.txt', 'z.log', '.hidden',
          's1/x.txt', 's2/y.txt', 'a-b/w.txt'] as $gsdF) {
    file_put_contents("$gsdDir/$gsdF", '');
}

echo "wrapper listed: ", var_export(in_array('glob', stream_get_wrappers()), true), "\n";

/* opendir/readdir/rewinddir hand out BASENAMES, in glob()'s order. */
$gsdH = opendir("glob://$gsdDir/*.txt");
$gsdSeen = [];
while (false !== ($gsdE = readdir($gsdH))) { $gsdSeen[] = $gsdE; }
echo "readdir      : ", json_encode($gsdSeen), "\n";
echo "past the end : ", var_export(readdir($gsdH), true), "\n";
rewinddir($gsdH);
echo "after rewind : ", var_export(readdir($gsdH), true), "\n";
closedir($gsdH);

/* No matches is an OPEN stream with nothing in it, not a failed open. */
$gsdH = opendir("glob://$gsdDir/*.nope");
echo "no matches   : ", var_export(is_resource($gsdH) || $gsdH instanceof Directory, true),
     ' ', var_export(readdir($gsdH), true), "\n";
closedir($gsdH);

/* scandir() and dir() ride the same two calls. */
echo "scandir      : ", json_encode(scandir("glob://$gsdDir/*.txt")), "\n";
$gsdD = dir("glob://$gsdDir/s?/*.txt");
echo "dir path     : ", var_export(str_replace($gsdDir, '<d>', $gsdD->path), true), "\n";
echo "dir reads    : ", json_encode([$gsdD->read(), $gsdD->read(), $gsdD->read()]), "\n";
$gsdD->close();

/* The wrapper has no stream opener and no stat. */
set_error_handler(function ($n, $s) use ($gsdDir) {
    echo '  WARN: ', str_replace($gsdDir, '<d>', $s), "\n";
    return true;
});
foreach (['fopen', 'file_get_contents', 'readfile'] as $gsdFn) {
    echo "$gsdFn:\n";
    var_dump($gsdFn === 'fopen' ? @$gsdFn("glob://$gsdDir/*.txt", 'r') : @$gsdFn("glob://$gsdDir/*.txt"));
}
restore_error_handler();
echo "file_exists  : ", var_export(@file_exists("glob://$gsdDir/*.txt"), true), "\n";
echo "is_dir       : ", var_export(@is_dir("glob://$gsdDir/*.txt"), true), "\n";

/* THE PIN: the device and glob() must answer the same matches in the same
 * order for every shape of pattern. A trailing slash is the one place they
 * part company, and php parts there too -- the entry of `d/` is the EMPTY
 * string, which a directory walk reads as the end. */
foreach (['*', '*.txt', '[0-9]*', '.*', 's?/*', '*/*.txt', 'a*/*', 'nope/*',
          '*.txt', 'z.log', '[!.]*', '*[0-9].txt'] as $gsdP) {
    $gsdWant = array_map('basename', glob("$gsdDir/$gsdP"));
    $gsdGot = [];
    $gsdH = opendir("glob://$gsdDir/$gsdP");
    while (false !== ($gsdE = readdir($gsdH))) { $gsdGot[] = $gsdE; }
    closedir($gsdH);
    echo str_pad($gsdP, 12), ' ', ($gsdWant === $gsdGot ? 'SAME' : 'DIFF'),
         ' ', json_encode($gsdGot), "\n";
}

foreach (['s1/x.txt', 's2/y.txt', 'a-b/w.txt', '1.txt', '2.txt', '10.txt',
          '9.txt', 'z.log', '.hidden'] as $gsdF) { @unlink("$gsdDir/$gsdF"); }
foreach (['s1', 's2', 'a-b'] as $gsdD2) { @rmdir("$gsdDir/$gsdD2"); }
@rmdir($gsdDir);
--EXPECT--
wrapper listed: true
readdir      : ["1.txt","10.txt","2.txt","9.txt"]
past the end : false
after rewind : '1.txt'
no matches   : true false
scandir      : ["1.txt","10.txt","2.txt","9.txt"]
dir path     : 'glob://<d>/s?/*.txt'
dir reads    : ["x.txt","y.txt",false]
fopen:
  WARN: fopen(glob://<d>/*.txt): Failed to open stream: wrapper does not support stream open
bool(false)
file_get_contents:
  WARN: file_get_contents(glob://<d>/*.txt): Failed to open stream: wrapper does not support stream open
bool(false)
readfile:
  WARN: readfile(glob://<d>/*.txt): Failed to open stream: wrapper does not support stream open
bool(false)
file_exists  : false
is_dir       : false
*            SAME ["1.txt","10.txt","2.txt","9.txt","a-b","s1","s2","z.log"]
*.txt        SAME ["1.txt","10.txt","2.txt","9.txt"]
[0-9]*       SAME ["1.txt","10.txt","2.txt","9.txt"]
.*           SAME [".","..",".hidden"]
s?/*         SAME ["x.txt","y.txt"]
*/*.txt      SAME ["w.txt","x.txt","y.txt"]
a*/*         SAME ["w.txt"]
nope/*       SAME []
*.txt        SAME ["1.txt","10.txt","2.txt","9.txt"]
z.log        SAME ["z.log"]
[!.]*        SAME ["1.txt","10.txt","2.txt","9.txt","a-b","s1","s2","z.log"]
*[0-9].txt   SAME ["1.txt","10.txt","2.txt","9.txt"]
