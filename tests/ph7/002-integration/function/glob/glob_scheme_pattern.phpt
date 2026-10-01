--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
glob() over a pattern carrying a scheme is glob(3) over the raw pattern
--DESCRIPTION--
php's glob() is the one directory reader that never consults a stream wrapper:
'zzz://*' names the directory 'zzz:' and the empty component after it, so the
matches under ./zzz:/ are listed with the pattern's own slashes kept --
'zzz://hit.php', 'zzz:///hit.php' for three -- and a scheme whose directory does
not exist (file://, glob://, any registered wrapper) is simply zero matches, in
silence. This engine answered [] for every scheme. The flags apply to the
rewritten pattern exactly as to a plain one.
--SKIPIF--
<?php
if (DIRECTORY_SEPARATOR === '\\') {
    die("skip a directory named 'zzz:' cannot exist on Windows");
}
?>
--FILE--
<?php
set_error_handler(function ($no, $msg) {
    if (!(error_reporting() & $no)) { return false; }
    echo "W: $msg\n"; return true;
});
$cwd = getcwd();
$dir = sys_get_temp_dir() . '/phl_glob_scheme_' . getmypid();
@mkdir($dir . '/zzz:/sub', 0777, true);
@mkdir($dir . '/pd');
file_put_contents($dir . '/zzz:/hit.php', "x\n");
file_put_contents($dir . '/zzz:/sub/a.txt', "x\n");
file_put_contents($dir . '/pd/f.txt', "x\n");
chdir($dir);

foreach (['zzz://*', 'zzz:///*', 'zzz://', 'zzz://hit.php', 'zzz://nope',
          'zzz://sub/*', 'zzz://*/*', 'zzz://s*/', 'zzz://*.php', 'zzz://.',
          'file://pd/*', 'file://' . $dir . '/pd/*', 'glob://pd/*', 'yyy://*'] as $pattern) {
    echo str_replace($dir, '<dir>', $pattern), ' => ', json_encode(glob($pattern)), "\n";
}
foreach ([['zzz://nope', GLOB_NOCHECK], ['zzz://*', GLOB_MARK], ['zzz://*', GLOB_ONLYDIR],
          ['zzz://{h,s}*', GLOB_BRACE], ['zzz://*', GLOB_NOSORT | GLOB_ERR]] as [$pattern, $flags]) {
    $r = glob($pattern, $flags);
    if ($flags & GLOB_NOSORT) { sort($r); }   // the directory's own order
    echo "$pattern, $flags => ", json_encode($r), "\n";
}

chdir($cwd);
unlink($dir . '/zzz:/sub/a.txt');
unlink($dir . '/zzz:/hit.php');
unlink($dir . '/pd/f.txt');
rmdir($dir . '/zzz:/sub');
rmdir($dir . '/zzz:');
rmdir($dir . '/pd');
rmdir($dir);
echo "done\n";
--EXPECT--
zzz://* => ["zzz:\/\/hit.php","zzz:\/\/sub"]
zzz:///* => ["zzz:\/\/\/hit.php","zzz:\/\/\/sub"]
zzz:// => ["zzz:\/\/"]
zzz://hit.php => ["zzz:\/\/hit.php"]
zzz://nope => []
zzz://sub/* => ["zzz:\/\/sub\/a.txt"]
zzz://*/* => ["zzz:\/\/sub\/a.txt"]
zzz://s*/ => ["zzz:\/\/sub\/"]
zzz://*.php => ["zzz:\/\/hit.php"]
zzz://. => ["zzz:\/\/."]
file://pd/* => []
file://<dir>/pd/* => []
glob://pd/* => []
yyy://* => []
zzz://nope, 16 => ["zzz:\/\/nope"]
zzz://*, 8 => ["zzz:\/\/hit.php","zzz:\/\/sub\/"]
zzz://*, 1073741824 => ["zzz:\/\/sub"]
zzz://{h,s}*, 128 => ["zzz:\/\/hit.php","zzz:\/\/sub"]
zzz://*, 36 => ["zzz:\/\/hit.php","zzz:\/\/sub"]
done
