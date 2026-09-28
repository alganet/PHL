--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A userland stream wrapper owns writes, metadata and directories on its own paths
--FILE--
<?php
set_error_handler(function ($n, $s) { echo "  ERR[$n] $s\n"; return true; });

class SwdWrapper
{
	public $context;
	public static $log = [];
	public static $ok = true;
	public static $entries = ['b.txt', 'a.txt', 'sub'];
	private $i = 0;
	public function unlink($p) { self::$log[] = "unlink($p)"; return self::$ok; }
	public function rename($a, $b) { self::$log[] = "rename($a,$b)"; return self::$ok; }
	public function mkdir($p, $m, $o) { self::$log[] = sprintf('mkdir(%s,%o,%d)', $p, $m, $o); return self::$ok; }
	public function rmdir($p, $o) { self::$log[] = "rmdir($p,$o)"; return self::$ok; }
	public function stream_metadata($p, $op, $v)
	{
		self::$log[] = "meta($p,$op," . str_replace(["\n", ' '], '', var_export($v, true)) . ')';
		return self::$ok;
	}
	public function dir_opendir($p, $o) { self::$log[] = "dir_opendir($p,$o)"; $this->i = 0; return self::$ok; }
	public function dir_readdir() { $e = self::$entries[$this->i++] ?? false; self::$log[] = 'dir_readdir=' . var_export($e, true); return $e; }
	public function dir_rewinddir() { self::$log[] = 'dir_rewinddir'; $this->i = 0; return true; }
	public function dir_closedir() { self::$log[] = 'dir_closedir'; return true; }
	public function stream_open($p, $m, $o, &$op) { self::$log[] = "stream_open($p,'$m',$o)"; return false; }
	public function url_stat($p, $f)
	{
		$a = ['dev' => 0, 'ino' => 0, 'mode' => 0100666, 'nlink' => 0, 'uid' => 0, 'gid' => 0,
		      'rdev' => 0, 'size' => 3, 'atime' => 1, 'mtime' => 2, 'ctime' => 3,
		      'blksize' => 0, 'blocks' => 0];
		return array_merge(array_values($a), $a);
	}
}
/* The same wrapper with none of the protocol's write or directory methods. */
class SwdBlind
{
	public $context;
	public function stream_open($p, $m, $o, &$op) { return false; }
}
stream_wrapper_register('swd', 'SwdWrapper');
stream_wrapper_register('swdblind', 'SwdBlind');

function swdRun($label, $fn)
{
	SwdWrapper::$log = [];
	echo $label, ': ';
	try { var_export($fn()); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(); }
	echo "\n   calls: ", implode(' | ', SwdWrapper::$log), "\n";
}

foreach ([true, false] as $swdOk) {
	SwdWrapper::$ok = $swdOk;
	echo '### the wrapper answers ', var_export($swdOk, true), "\n";
	swdRun('unlink', fn () => unlink('swd://u'));
	swdRun('rename', fn () => rename('swd://x', 'swd://y'));
	swdRun('mkdir', fn () => mkdir('swd://m'));
	swdRun('mkdir 0700 recursive', fn () => mkdir('swd://m2', 0700, true));
	swdRun('rmdir', fn () => rmdir('swd://r'));
	swdRun('touch', fn () => touch('swd://t'));
	swdRun('touch mtime', fn () => touch('swd://t', 1234567));
	swdRun('touch mtime+atime', fn () => touch('swd://t', 1234567, 7654321));
	swdRun('chmod', fn () => chmod('swd://c', 0755));
	swdRun('chown id', fn () => chown('swd://c', 4242));
	swdRun('chown name', fn () => chown('swd://c', 'nobody'));
	swdRun('chgrp id', fn () => chgrp('swd://c', 4243));
	swdRun('chgrp name', fn () => chgrp('swd://c', 'nogroup'));
}
SwdWrapper::$ok = true;

/* php refuses a rename whose two ends are not the same wrapper's -- but a scheme
 * nothing is registered under is a different sentence, and the rename is still
 * attempted (and fails) on the plain path. */
swdRun('rename across wrappers', fn () => rename('swd://x', 'swdblind://y'));
swdRun('rename unknown scheme', fn () => rename('swd://x', 'swdnosuch://y'));

/* The directory door. */
swdRun('opendir/readdir', function () {
	$d = opendir('swd://root'); $o = [];
	while (($e = readdir($d)) !== false) { $o[] = $e; }
	closedir($d);
	return $o;
});
swdRun('rewinddir', function () {
	$d = opendir('swd://root'); readdir($d); rewinddir($d);
	$e = readdir($d); closedir($d);
	return $e;
});
swdRun('scandir', fn () => scandir('swd://root'));
swdRun('scandir none', fn () => scandir('swd://root', SCANDIR_SORT_NONE));
swdRun('dir()', function () {
	$d = dir('swd://root'); $o = [];
	while (($e = $d->read()) !== false) { $o[] = $e; }
	$d->close();
	return [$d->path, $o];
});
swdRun('DirectoryIterator', function () {
	$o = [];
	foreach (new DirectoryIterator('swd://root') as $f) { $o[] = $f->getFilename(); }
	return $o;
});
/* glob() is the one directory reader php does NOT route through a wrapper. */
swdRun('glob', fn () => glob('swd://root/*'));

SwdWrapper::$ok = false;
swdRun('opendir refused', fn () => opendir('swd://root'));
SwdWrapper::$ok = true;

/* The mode STRING php hands stream_open: the caller's own spelling from fopen(),
 * and the C openers' fixed ones everywhere else. */
foreach (['r', 'rb', 'r+', 'w', 'wb', 'w+', 'a', 'a+', 'x', 'x+', 'c', 'c+', 'w+b'] as $swdMode) {
	swdRun("fopen $swdMode", fn () => fopen('swd://f', $swdMode));
}
swdRun('file_get_contents', fn () => file_get_contents('swd://f'));
swdRun('file_put_contents', fn () => file_put_contents('swd://f', 'x'));
swdRun('file_put_contents append', fn () => file_put_contents('swd://f', 'x', FILE_APPEND));
swdRun('file', fn () => file('swd://f'));
swdRun('readfile', fn () => readfile('swd://f'));

/* A wrapper that implements none of it: php names the method it wanted. */
foreach ([['unlink', fn () => unlink('swdblind://u')],
          ['rename', fn () => rename('swdblind://x', 'swdblind://y')],
          ['mkdir', fn () => mkdir('swdblind://m')],
          ['rmdir', fn () => rmdir('swdblind://r')],
          ['touch', fn () => touch('swdblind://t')],
          ['chmod', fn () => chmod('swdblind://c', 0755)],
          ['opendir', fn () => opendir('swdblind://d')]] as [$swdName, $swdFn]) {
	echo "blind $swdName: ";
	var_export($swdFn());
	echo "\n";
}

stream_wrapper_unregister('swd');
stream_wrapper_unregister('swdblind');
restore_error_handler();
?>
--EXPECT--
### the wrapper answers true
unlink: true
   calls: unlink(swd://u)
rename: true
   calls: rename(swd://x,swd://y)
mkdir: true
   calls: mkdir(swd://m,777,8)
mkdir 0700 recursive: true
   calls: mkdir(swd://m2,700,9)
rmdir: true
   calls: rmdir(swd://r,8)
touch: true
   calls: meta(swd://t,1,array())
touch mtime: true
   calls: meta(swd://t,1,array(0=>1234567,1=>1234567,))
touch mtime+atime: true
   calls: meta(swd://t,1,array(0=>1234567,1=>7654321,))
chmod: true
   calls: meta(swd://c,6,493)
chown id: true
   calls: meta(swd://c,3,4242)
chown name: true
   calls: meta(swd://c,2,'nobody')
chgrp id: true
   calls: meta(swd://c,5,4243)
chgrp name: true
   calls: meta(swd://c,4,'nogroup')
### the wrapper answers false
unlink: false
   calls: unlink(swd://u)
rename: false
   calls: rename(swd://x,swd://y)
mkdir: false
   calls: mkdir(swd://m,777,8)
mkdir 0700 recursive: false
   calls: mkdir(swd://m2,700,9)
rmdir: false
   calls: rmdir(swd://r,8)
touch: false
   calls: meta(swd://t,1,array())
touch mtime: false
   calls: meta(swd://t,1,array(0=>1234567,1=>1234567,))
touch mtime+atime: false
   calls: meta(swd://t,1,array(0=>1234567,1=>7654321,))
chmod: false
   calls: meta(swd://c,6,493)
chown id: false
   calls: meta(swd://c,3,4242)
chown name: false
   calls: meta(swd://c,2,'nobody')
chgrp id: false
   calls: meta(swd://c,5,4243)
chgrp name: false
   calls: meta(swd://c,4,'nogroup')
rename across wrappers:   ERR[2] rename(): Cannot rename a file across wrapper types
false
   calls: 
rename unknown scheme:   ERR[2] rename(): Unable to find the wrapper "swdnosuch" - did you forget to enable it when you configured PHP?
  ERR[2] rename(): Cannot rename a file across wrapper types
false
   calls: 
opendir/readdir: array (
  0 => 'b.txt',
  1 => 'a.txt',
  2 => 'sub',
)
   calls: dir_opendir(swd://root,0) | dir_readdir='b.txt' | dir_readdir='a.txt' | dir_readdir='sub' | dir_readdir=false | dir_closedir
rewinddir: 'b.txt'
   calls: dir_opendir(swd://root,0) | dir_readdir='b.txt' | dir_rewinddir | dir_readdir='b.txt' | dir_closedir
scandir: array (
  0 => 'a.txt',
  1 => 'b.txt',
  2 => 'sub',
)
   calls: dir_opendir(swd://root,0) | dir_readdir='b.txt' | dir_readdir='a.txt' | dir_readdir='sub' | dir_readdir=false | dir_closedir
scandir none: array (
  0 => 'b.txt',
  1 => 'a.txt',
  2 => 'sub',
)
   calls: dir_opendir(swd://root,0) | dir_readdir='b.txt' | dir_readdir='a.txt' | dir_readdir='sub' | dir_readdir=false | dir_closedir
dir(): array (
  0 => 'swd://root',
  1 => 
  array (
    0 => 'b.txt',
    1 => 'a.txt',
    2 => 'sub',
  ),
)
   calls: dir_opendir(swd://root,0) | dir_readdir='b.txt' | dir_readdir='a.txt' | dir_readdir='sub' | dir_readdir=false | dir_closedir
DirectoryIterator: array (
  0 => 'b.txt',
  1 => 'a.txt',
  2 => 'sub',
)
   calls: dir_opendir(swd://root,0) | dir_readdir='b.txt' | dir_rewinddir | dir_readdir='b.txt' | dir_readdir='a.txt' | dir_readdir='sub' | dir_readdir=false | dir_closedir
glob: array (
)
   calls: 
opendir refused:   ERR[2] opendir(swd://root): Failed to open directory: "SwdWrapper::dir_opendir" call failed
false
   calls: dir_opendir(swd://root,0)
fopen r:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'r',0)
fopen rb:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'rb',0)
fopen r+:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'r+',0)
fopen w:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'w',0)
fopen wb:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'wb',0)
fopen w+:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'w+',0)
fopen a:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'a',0)
fopen a+:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'a+',0)
fopen x:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'x',0)
fopen x+:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'x+',0)
fopen c:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'c',0)
fopen c+:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'c+',0)
fopen w+b:   ERR[2] fopen(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'w+b',0)
file_get_contents:   ERR[2] file_get_contents(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'rb',0)
file_put_contents:   ERR[2] file_put_contents(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'wb',0)
file_put_contents append:   ERR[2] file_put_contents(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'ab',0)
file:   ERR[2] file(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'rb',0)
readfile:   ERR[2] readfile(swd://f): Failed to open stream: "SwdWrapper::stream_open" call failed
false
   calls: stream_open(swd://f,'rb',0)
blind unlink:   ERR[2] unlink(): SwdBlind::unlink is not implemented!
false
blind rename:   ERR[2] rename(): SwdBlind::rename is not implemented!
false
blind mkdir:   ERR[2] mkdir(): SwdBlind::mkdir is not implemented!
false
blind rmdir:   ERR[2] rmdir(): SwdBlind::rmdir is not implemented!
false
blind touch:   ERR[2] touch(): SwdBlind::stream_metadata is not implemented!
false
blind chmod:   ERR[2] chmod(): SwdBlind::stream_metadata is not implemented!
false
blind opendir:   ERR[2] opendir(swdblind://d): Failed to open directory: "SwdBlind::dir_opendir" is not implemented
false
