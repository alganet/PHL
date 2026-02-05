--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_link() should return true for a symlink and false for regular file
--SKIPIF--
<?php
if (PHP_OS == 'WINNT') {
    echo "skip: platform";
}
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_islink_');
file_put_contents($fname, 'Hello');
$sym = $fname . '.lnk';
$ok = symlink($fname, $sym);
if ($ok) {
    echo "is_link_sym=" . (is_link($sym) ? 'true' : 'false') . PHP_EOL;
    echo "is_link_file=" . (is_link($fname) ? 'true' : 'false') . PHP_EOL;
} else {
    echo "symlink_failed\n";
}
?>
--EXPECTF--
is_link_sym=%s
is_link_file=false
--CLEAN--
<?php
@unlink($sym);
@unlink($fname);
unset($fname, $sym, $ok);
