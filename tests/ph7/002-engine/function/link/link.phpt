--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
link should create a hard link to a file
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' && !function_exists('zend_version')) {
    echo "skip: platform";
}
?>
--FILE--
<?php
$src = tempnam(sys_get_temp_dir(), 'ph7_link_src_');
file_put_contents($src, 'HARDLINK');
$dst = $src . '.dst';
$ok = link($src, $dst);
echo "link_ok=" . ($ok ? 'true' : 'false') . PHP_EOL;
echo "dst_exists=" . (file_exists($dst) ? 'true' : 'false') . PHP_EOL;
echo "same_content=" . (file_get_contents($dst) === 'HARDLINK' ? 'true' : 'false') . PHP_EOL;
?>
--EXPECT--
link_ok=true
dst_exists=true
same_content=true
--CLEAN--
<?php
@unlink($dst);
@unlink($src);
?>
