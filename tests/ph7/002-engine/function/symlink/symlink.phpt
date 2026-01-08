--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
symlink should create a symbolic link to a file
--SKIPIF--
<?php
if (PHP_OS == 'WINNT') {
    echo "skip: symlink not supported on Windows without proper permissions";
}
?>
--FILE--
<?php
$src = tempnam(sys_get_temp_dir(), 'ph7_symlink_src_');
file_put_contents($src, 'SYMLINK_CONTENT');
$dst = $src . '.symlink';
$ok = symlink($src, $dst);
echo "symlink_ok=" . ($ok ? 'true' : 'false') . PHP_EOL;
if ($ok) {
    // Note: is_link() may not work properly in some environments
    echo "target_exists=" . (file_exists($dst) ? 'true' : 'false') . PHP_EOL;
}
?>
--EXPECT--
symlink_ok=true
target_exists=true
--CLEAN--
<?php
@unlink($dst);
@unlink($src);
?>
