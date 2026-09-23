--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The SPL path constructors refuse a NUL byte the way the path builtins do
--SKIPIF--
skip: flaky
--FILE--
<?php
/* Same Z_PARAM_PATH rule as function/fopen/nul_byte_path_argument.phpt, reported
 * under php's QUALIFIED name. These are native methods, so the same central
 * screen decides them -- they were simply not on its table, and the truncated
 * path reached the VFS. */
$p = sys_get_temp_dir() . DIRECTORY_SEPARATOR . "phl_splnul_aa\0bb";

$cases = [
	'SplFileInfo'                => fn() => new SplFileInfo($p),
	'DirectoryIterator'          => fn() => new DirectoryIterator($p),
	'FilesystemIterator'         => fn() => new FilesystemIterator($p),
	'RecursiveDirectoryIterator' => fn() => new RecursiveDirectoryIterator($p),
];
foreach ($cases as $name => $fn) {
	echo str_pad($name, 28);
	try {
		$o = $fn();
		echo 'NO THROW: ', get_debug_type($o);
	} catch (ValueError $e) {
		echo $e->getMessage();
	}
	echo "\n";
}

/* A clean path still builds. (Asserted through isDir() rather than a
 * getPathname() round-trip: a Windows temp dir comes back with its trailing
 * separator handled differently, which is not what this test is about.) */
$dir = sys_get_temp_dir();
echo 'clean SplFileInfo: ', var_export((new SplFileInfo($dir))->isDir(), true), "\n";
echo 'clean DirectoryIterator: ', var_export((new DirectoryIterator($dir))->isDir(), true), "\n";
?>
--EXPECT--
SplFileInfo                 SplFileInfo::__construct(): Argument #1 ($filename) must not contain any null bytes
DirectoryIterator           DirectoryIterator::__construct(): Argument #1 ($directory) must not contain any null bytes
FilesystemIterator          FilesystemIterator::__construct(): Argument #1 ($directory) must not contain any null bytes
RecursiveDirectoryIterator  RecursiveDirectoryIterator::__construct(): Argument #1 ($directory) must not contain any null bytes
clean SplFileInfo: true
clean DirectoryIterator: true
