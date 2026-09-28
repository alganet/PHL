--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The stat family asks a userland stream wrapper's url_stat()
--FILE--
<?php
set_error_handler(function ($n, $s) { echo "  ERR[$n] $s\n"; return true; });

class UrlStatWrapper
{
	public $context;
	public static $table = [];
	public static $flags = [];
	public function url_stat($path, $flags)
	{
		self::$flags[] = $flags;
		return self::$table[$path] ?? false;
	}
	public function stream_open($path, $mode, $options, &$opened) { return false; }
}
/* A wrapper with no url_stat at all: php says so, and says it even for the
 * questions it otherwise asks in silence. */
class UrlStatBlind
{
	public $context;
	public function stream_open($path, $mode, $options, &$opened) { return false; }
}
stream_wrapper_register('usw', 'UrlStatWrapper');
stream_wrapper_register('uswblind', 'UrlStatBlind');

function urlStatRecord($mode, $uid, $gid, $size = 7)
{
	$a = ['dev' => 1, 'ino' => 2, 'mode' => $mode, 'nlink' => 3, 'uid' => $uid, 'gid' => $gid,
	      'rdev' => 4, 'size' => $size, 'atime' => 111, 'mtime' => 222, 'ctime' => 333,
	      'blksize' => 512, 'blocks' => 9];
	/* What a real wrapper returns: the thirteen values numbered, then named. */
	return array_merge(array_values($a), $a);
}

/* The record's uid IS the process's, so php reads the OWNER bits -- the one
 * choice of mask that is the same on every platform (php's Windows build makes
 * no uid comparison at all and reads the owner bits unconditionally). */
$urlStatUid = getmyuid();
$urlStatGid = getmygid();
UrlStatWrapper::$table = [
	'usw://reg'  => urlStatRecord(0100644, $urlStatUid, $urlStatGid),
	'usw://dir'  => urlStatRecord(0040755, $urlStatUid, $urlStatGid, 0),
	'usw://link' => urlStatRecord(0120777, $urlStatUid, $urlStatGid),
	'usw://none' => urlStatRecord(0100000, $urlStatUid, $urlStatGid),
	/* Every permission bit set, so owner/group/other all answer alike. */
	'usw://open' => urlStatRecord(0100777, 4242, 4243),
	/* Named keys only, and only one of them: php defaults the rest to 0. */
	'usw://partial' => ['size' => 9],
	/* Numeric keys only: php reads the NAMED half, so this record is all zeros. */
	'usw://numonly' => [0, 0, 0100600, 0, 0, 0, 0, 42, 1, 2, 3, 0, 0],
];

$urlStatFns = ['file_exists', 'is_file', 'is_dir', 'is_link', 'is_readable', 'is_writable',
               'is_executable', 'filesize', 'filemtime', 'fileatime', 'filectime',
               'fileinode', 'fileperms', 'filetype'];
foreach (['usw://reg', 'usw://dir', 'usw://link', 'usw://none', 'usw://open',
          'usw://partial', 'usw://numonly', 'usw://missing'] as $p) {
	echo "== $p\n";
	foreach ($urlStatFns as $f) {
		echo "  $f: ", var_export($f($p), true), "\n";
	}
}

/* fileowner()/filegroup() are asked apart from the loop: the process's own uid and
 * gid differ between machines (and are both 0 on the Windows gate), so the record
 * that carries them is compared for IDENTITY and a record that carries neither is
 * read for its literal number. */
echo "owner/group open: ", fileowner('usw://open'), ' ', filegroup('usw://open'), "\n";
echo "owner/group partial: ", fileowner('usw://partial'), ' ', filegroup('usw://partial'), "\n";
echo "owner/group reg is the process's: ",
	var_export(fileowner('usw://reg') === $urlStatUid, true), ' ',
	var_export(filegroup('usw://reg') === $urlStatGid, true), "\n";
echo "owner/group missing: ", var_export(@fileowner('usw://missing'), true), ' ',
	var_export(@filegroup('usw://missing'), true), "\n";

/* stat()/lstat(): php's thirteen fields numbered 0..12 and then named. */
$urlStatS = stat('usw://reg');
echo "stat count: ", count($urlStatS), "\n";
$urlStatS[4] = $urlStatS[4] === $urlStatUid ? 'OWN-UID' : $urlStatS[4];
$urlStatS[5] = $urlStatS[5] === $urlStatGid ? 'OWN-GID' : $urlStatS[5];
/* php's Windows stat record has no blksize/blocks: it never reads the wrapper's
 * pair and reports -1 for both, so -1 is printed as the wrapper's own answer */
$urlStatWin = DIRECTORY_SEPARATOR === '\\';
foreach ([11 => 512, 12 => 9, 'blksize' => 512, 'blocks' => 9] as $urlStatK => $urlStatV) {
    $urlStatS[$urlStatK] = $urlStatWin && $urlStatS[$urlStatK] === -1 ? $urlStatV : $urlStatS[$urlStatK];
}
echo "stat 0..12: ", implode(',', array_slice($urlStatS, 0, 13)), "\n";
echo "stat named size/mode/blocks: ", $urlStatS['size'], ' ', $urlStatS['mode'], ' ', $urlStatS['blocks'], "\n";
echo "lstat size: ", var_export(lstat('usw://link')['size'], true), "\n";
echo "stat missing: ", var_export(stat('usw://missing'), true), "\n";
echo "lstat missing: ", var_export(lstat('usw://missing'), true), "\n";

/* The FLAGS php hands the wrapper: LINK for the lstat readers, QUIET for the
 * existence and access questions. (The 4 bit is php's NOCACHE, which its own stat
 * cache sets on every ask the family makes.) */
foreach (['file_exists', 'is_file', 'is_link', 'filesize', 'filetype', 'stat', 'lstat'] as $f) {
	clearstatcache();  /* php's own one-entry stat cache would answer instead */
	UrlStatWrapper::$flags = [];
	$f('usw://reg');
	echo "flags $f: ", implode(',', array_unique(UrlStatWrapper::$flags)), "\n";
}

/* SplFileInfo asks the same door. */
foreach (['usw://reg', 'usw://dir', 'usw://link'] as $p) {
	$i = new SplFileInfo($p);
	echo "spl $p: ", $i->getSize(), ' ', var_export($i->isFile(), true), ' ',
		var_export($i->isDir(), true), ' ', var_export($i->isLink(), true), ' ',
		var_export($i->isReadable(), true), ' ', $i->getMTime(), ' ', $i->getPerms(),
		' ', $i->getType(), "\n";
}
try { (new SplFileInfo('usw://missing'))->getSize(); }
catch (Throwable $e) { echo 'spl missing: ', get_class($e), ': ', $e->getMessage(), "\n"; }

/* A wrapper that does not implement url_stat. */
echo "blind exists: ", var_export(file_exists('uswblind://x'), true), "\n";
echo "blind size: ", var_export(filesize('uswblind://x'), true), "\n";

/* A plain path is untouched by any of this. */
echo "plain: ", var_export(is_dir(sys_get_temp_dir()), true), ' ',
	var_export(file_exists(sys_get_temp_dir() . '/no-such-thing-here'), true), "\n";

/* A wrapper whose url_stat THROWS: the exception IS the answer, and nothing may
 * stack another on top of it -- SplFileInfo used to raise its own RuntimeException
 * over this one, leaving the first caught and the second uncaught. Only the QUIET
 * asks are exercised here: the rest raise a warning of their own beside the throw,
 * and a C builtin's throw reaches the catch before that warning is written. */
class UrlStatThrower
{
	public $context;
	public function url_stat($path, $flags) { throw new DomainException('from url_stat'); }
}
stream_wrapper_register('uswthrow', 'UrlStatThrower');
foreach ([fn () => is_file('uswthrow://x'),
          fn () => is_dir('uswthrow://x'),
          fn () => (new SplFileInfo('uswthrow://x'))->isFile(),
          fn () => (new SplFileInfo('uswthrow://x'))->getSize()] as $i => $probe) {
	try { $probe(); echo "throw $i: no exception\n"; }
	catch (Throwable $e) { echo "throw $i: ", get_class($e), ': ', $e->getMessage(), "\n"; }
}
stream_wrapper_unregister('uswthrow');

stream_wrapper_unregister('usw');
stream_wrapper_unregister('uswblind');
restore_error_handler();
?>
--EXPECT--
== usw://reg
  file_exists: true
  is_file: true
  is_dir: false
  is_link: false
  is_readable: true
  is_writable: true
  is_executable: false
  filesize: 7
  filemtime: 222
  fileatime: 111
  filectime: 333
  fileinode: 2
  fileperms: 33188
  filetype: 'file'
== usw://dir
  file_exists: true
  is_file: false
  is_dir: true
  is_link: false
  is_readable: true
  is_writable: true
  is_executable: true
  filesize: 0
  filemtime: 222
  fileatime: 111
  filectime: 333
  fileinode: 2
  fileperms: 16877
  filetype: 'dir'
== usw://link
  file_exists: true
  is_file: false
  is_dir: false
  is_link: true
  is_readable: true
  is_writable: true
  is_executable: true
  filesize: 7
  filemtime: 222
  fileatime: 111
  filectime: 333
  fileinode: 2
  fileperms: 41471
  filetype: 'link'
== usw://none
  file_exists: true
  is_file: true
  is_dir: false
  is_link: false
  is_readable: false
  is_writable: false
  is_executable: false
  filesize: 7
  filemtime: 222
  fileatime: 111
  filectime: 333
  fileinode: 2
  fileperms: 32768
  filetype: 'file'
== usw://open
  file_exists: true
  is_file: true
  is_dir: false
  is_link: false
  is_readable: true
  is_writable: true
  is_executable: true
  filesize: 7
  filemtime: 222
  fileatime: 111
  filectime: 333
  fileinode: 2
  fileperms: 33279
  filetype: 'file'
== usw://partial
  file_exists: true
  is_file: false
  is_dir: false
  is_link: false
  is_readable: false
  is_writable: false
  is_executable: false
  filesize: 9
  filemtime: 0
  fileatime: 0
  filectime: 0
  fileinode: 0
  fileperms: 0
  filetype:   ERR[8] filetype(): Unknown file type (0)
'unknown'
== usw://numonly
  file_exists: true
  is_file: false
  is_dir: false
  is_link: false
  is_readable: false
  is_writable: false
  is_executable: false
  filesize: 0
  filemtime: 0
  fileatime: 0
  filectime: 0
  fileinode: 0
  fileperms: 0
  filetype:   ERR[8] filetype(): Unknown file type (0)
'unknown'
== usw://missing
  file_exists: false
  is_file: false
  is_dir: false
  is_link: false
  is_readable: false
  is_writable: false
  is_executable: false
  filesize:   ERR[2] filesize(): stat failed for usw://missing
false
  filemtime:   ERR[2] filemtime(): stat failed for usw://missing
false
  fileatime:   ERR[2] fileatime(): stat failed for usw://missing
false
  filectime:   ERR[2] filectime(): stat failed for usw://missing
false
  fileinode:   ERR[2] fileinode(): stat failed for usw://missing
false
  fileperms:   ERR[2] fileperms(): stat failed for usw://missing
false
  filetype:   ERR[2] filetype(): Lstat failed for usw://missing
false
owner/group open: 4242 4243
owner/group partial: 0 0
owner/group reg is the process's: true true
owner/group missing:   ERR[2] fileowner(): stat failed for usw://missing
false   ERR[2] filegroup(): stat failed for usw://missing
false
stat count: 26
stat 0..12: 1,2,33188,3,OWN-UID,OWN-GID,4,7,111,222,333,512,9
stat named size/mode/blocks: 7 33188 9
lstat size: 7
stat missing:   ERR[2] stat(): stat failed for usw://missing
false
lstat missing:   ERR[2] lstat(): Lstat failed for usw://missing
false
flags file_exists: 6
flags is_file: 6
flags is_link: 7
flags filesize: 4
flags filetype: 5
flags stat: 4
flags lstat: 5
spl usw://reg: 7 true false false true 222 33188 file
spl usw://dir: 0 false true false true 222 16877 dir
spl usw://link: 7 false false true true 222 41471 link
spl missing: RuntimeException: SplFileInfo::getSize(): stat failed for usw://missing
blind exists:   ERR[2] file_exists(): UrlStatBlind::url_stat is not implemented!
false
blind size:   ERR[2] filesize(): UrlStatBlind::url_stat is not implemented!
  ERR[2] filesize(): stat failed for uswblind://x
false
plain: true false
throw 0: DomainException: from url_stat
throw 1: DomainException: from url_stat
throw 2: DomainException: from url_stat
throw 3: DomainException: from url_stat
