--TEST--
readdir/scandir yield '.' and '..' (php parity); glob returns paths with the pattern's directory prefix, honours GLOB_ONLYDIR and the leading-dot rule
--FILE--
<?php
$rddDir = sys_get_temp_dir() . '/phl_rdd_' . getmypid();
@mkdir($rddDir);
@mkdir($rddDir . '/sub');
file_put_contents($rddDir . '/a.txt', 'a');
file_put_contents($rddDir . '/b.log', 'bb');
file_put_contents($rddDir . '/.hidden', 'h');
// readdir includes . and ..
$rddH = opendir($rddDir); $rddR = [];
while (($e = readdir($rddH)) !== false) { $rddR[] = $e; }
closedir($rddH); sort($rddR);
echo "readdir: ", implode(',', $rddR), "\n";
// scandir includes . and .. (sorted)
echo "scandir: ", implode(',', scandir($rddDir)), "\n";
// glob returns paths carrying the pattern's directory prefix, no dot-entries
$rddG = glob($rddDir . '/*');
echo "glob *: ", implode(',', array_map(fn($p) => substr($p, strlen($rddDir) + 1), $rddG)), "\n";
// glob GLOB_ONLYDIR
$rddGd = glob($rddDir . '/*', GLOB_ONLYDIR);
echo "glob ONLYDIR: ", implode(',', array_map(fn($p) => substr($p, strlen($rddDir) + 1), $rddGd)), "\n";
// glob '.*' matches dot entries
$rddGh = glob($rddDir . '/.*');
echo "glob .*: ", implode(',', array_map(fn($p) => substr($p, strlen($rddDir) + 1), $rddGh)), "\n";
--EXPECT--
readdir: .,..,.hidden,a.txt,b.log,sub
scandir: .,..,.hidden,a.txt,b.log,sub
glob *: a.txt,b.log,sub
glob ONLYDIR: sub
glob .*: .,..,.hidden
--CLEAN--
<?php
$rddDir = sys_get_temp_dir() . '/phl_rdd_' . getmypid();
@unlink($rddDir . '/a.txt');
@unlink($rddDir . '/b.log');
@unlink($rddDir . '/.hidden');
@rmdir($rddDir . '/sub');
@rmdir($rddDir);
