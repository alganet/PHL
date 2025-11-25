--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unlink and delete should remove files
--FILE--
<?php
$src = tempnam(sys_get_temp_dir(), 'ph7_unlink_');
file_put_contents($src, 'DELETE');
$ok = unlink($src);
echo "unlink_ok=" . ($ok ? 'true' : 'false') . PHP_EOL;
echo "exists_after_unlink=" . (file_exists($src) ? 'true' : 'false') . PHP_EOL;
$src2 = tempnam(sys_get_temp_dir(), 'ph7_unlink2_');
file_put_contents($src2, 'DELETE2');
?>
--EXPECT--
unlink_ok=true
exists_after_unlink=false
--CLEAN--
<?php
@unlink($src);
?>
