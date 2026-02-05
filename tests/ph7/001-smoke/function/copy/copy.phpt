--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
copy() should duplicate a file and keep the contents
--FILE--
<?php
$src = tempnam(sys_get_temp_dir(), 'ph7_copy_src_');
file_put_contents($src, 'COPYME');
$dst = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'ph7_copy_dst_' . uniqid() . '.txt';
$ok = copy($src, $dst);
echo "copied=" . ($ok ? 'true' : 'false') . PHP_EOL;
if ($ok) {
    echo "dst_exists=" . (file_exists($dst) ? 'true' : 'false') . PHP_EOL;
    echo "content=" . file_get_contents($dst) . PHP_EOL;
}
?>
--EXPECT--
copied=true
dst_exists=true
content=COPYME
--CLEAN--
<?php
@unlink($src);
@unlink($dst);
unset($src, $dst, $ok);
