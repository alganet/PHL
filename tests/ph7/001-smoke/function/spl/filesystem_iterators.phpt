--TEST--
SplFileInfo / DirectoryIterator / FilesystemIterator / RecursiveDirectoryIterator over a real directory tree
--SKIPIF--
skip: path issues on win
--FILE--
<?php
$fsiDir = sys_get_temp_dir() . '/phl_fsi_' . getmypid();
@mkdir($fsiDir);
@mkdir($fsiDir . '/sub');
file_put_contents($fsiDir . '/a.txt', 'a');
file_put_contents($fsiDir . '/b.log', 'bb');
file_put_contents($fsiDir . '/sub/c.txt', 'c');
// SplFileInfo
$fsiFi = new SplFileInfo($fsiDir . '/a.txt');
echo "SFI: ", $fsiFi->getFilename(), '|', $fsiFi->getBasename('.txt'), '|', $fsiFi->getExtension(),
     '|', var_export($fsiFi->isFile(), true), '|', var_export($fsiFi->isDir(), true), '|', $fsiFi->getSize(), "\n";
// DirectoryIterator (includes dots, current() is the iterator itself)
$fsiDi = [];
foreach (new DirectoryIterator($fsiDir) as $e) {
    $fsiDi[] = ($e->isDot() ? 'DOT:' : '') . $e->getFilename();
}
sort($fsiDi); echo "DI: ", implode(',', $fsiDi), "\n";
// FilesystemIterator (SKIP_DOTS default, current() is an SplFileInfo)
$fsiFsi = [];
foreach (new FilesystemIterator($fsiDir) as $k => $v) {
    $fsiFsi[] = $v->getFilename() . ':' . get_class($v);
}
sort($fsiFsi); echo "FSI: ", implode(',', $fsiFsi), "\n";
// RecursiveDirectoryIterator + RecursiveIteratorIterator (recursive leaves)
$fsiRdi = new RecursiveDirectoryIterator($fsiDir, FilesystemIterator::SKIP_DOTS);
$fsiAll = [];
foreach (new RecursiveIteratorIterator($fsiRdi) as $f) {
    $fsiAll[] = substr($f->getPathname(), strlen($fsiDir) + 1);
}
sort($fsiAll); echo "RII: ", implode(',', $fsiAll), "\n";
--EXPECT--
SFI: a.txt|a|txt|true|false|1
DI: DOT:.,DOT:..,a.txt,b.log,sub
FSI: a.txt:SplFileInfo,b.log:SplFileInfo,sub:SplFileInfo
RII: a.txt,b.log,sub/c.txt
--CLEAN--
<?php
$fsiDir = sys_get_temp_dir() . '/phl_fsi_' . getmypid();
@unlink($fsiDir . '/a.txt');
@unlink($fsiDir . '/b.log');
@unlink($fsiDir . '/sub/c.txt');
@rmdir($fsiDir . '/sub');
@rmdir($fsiDir);
