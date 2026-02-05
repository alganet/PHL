--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
umask should return previous mask and set new one
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' && !function_exists('zend_version')) {
    echo "skip: platform";
}
--FILE--
<?php
$orig = umask();
$old = umask(0022);
echo "old_mask=" . decoct($old) . PHP_EOL;
echo "new_mask=" . decoct(umask()) . PHP_EOL;
?>
--EXPECTF--
old_mask=%s
new_mask=%s
--CLEAN--
<?php
umask($orig);
unset($orig, $old);
