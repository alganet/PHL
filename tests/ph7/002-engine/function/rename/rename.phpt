--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rename() should move a file to a new name
--FILE--
<?php
$src = tempnam(sys_get_temp_dir(), 'ph7_rename_src_');
file_put_contents($src, 'HELLO');
$dst = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'ph7_rename_dst_' . uniqid() . '.txt';
$ok = rename($src, $dst);

echo "renamed=" . ($ok ? 'true' : 'false') . PHP_EOL;
if ($ok) {
    echo "dst_exists=" . (file_exists($dst) ? 'true' : 'false') . PHP_EOL;
    echo "src_exists=" . (file_exists($src) ? 'true' : 'false') . PHP_EOL;
    echo "content=" . file_get_contents($dst) . PHP_EOL;
}
?>
--EXPECT--
renamed=true
dst_exists=true
src_exists=false
content=HELLO
--CLEAN--
<?php
@unlink($src);
@unlink($dst);
unset($src, $dst, $ok);
?>
