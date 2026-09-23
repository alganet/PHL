--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The POSIX-only path builtins refuse a NUL byte too (link/symlink/chown/chgrp/chroot)
--SKIPIF--
skip: win macos for now
--FILE--
<?php
/* Same Z_PARAM_PATH rule as function/fopen/nul_byte_path_argument.phpt, on the
 * builtins php only ships on POSIX. These are the ones where a truncation is
 * most damaging: symlink() used to CREATE a link to the truncated target. */
$dir = sys_get_temp_dir();
$p   = $dir . DIRECTORY_SEPARATOR . "phl_nulp_aa\0bb";
$ok  = $dir . DIRECTORY_SEPARATOR . 'phl_nulp_src';

$cases = [
	'link #1'    => fn() => link($p, $ok),
	'link #2'    => fn() => link($ok, $p),
	'symlink #1' => fn() => symlink($p, $ok),
	'symlink #2' => fn() => symlink($ok, $p),
	'chown'      => fn() => chown($p, 0),
	'chgrp'      => fn() => chgrp($p, 0),
	'chroot'     => fn() => chroot($p),
];
foreach ($cases as $name => $fn) {
	echo str_pad($name, 12);
	try {
		$r = $fn();
		echo 'NO THROW: ', var_export($r, true);
	} catch (ValueError $e) {
		echo $e->getMessage();
	}
	echo "\n";
}
echo "src exists: ", var_export(file_exists($ok), true), "\n";
?>
--EXPECT--
link #1     link(): Argument #1 ($target) must not contain any null bytes
link #2     link(): Argument #2 ($link) must not contain any null bytes
symlink #1  symlink(): Argument #1 ($target) must not contain any null bytes
symlink #2  symlink(): Argument #2 ($link) must not contain any null bytes
chown       chown(): Argument #1 ($filename) must not contain any null bytes
chgrp       chgrp(): Argument #1 ($filename) must not contain any null bytes
chroot      chroot(): Argument #1 ($directory) must not contain any null bytes
src exists: false
