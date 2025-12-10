--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
md5_file returns known hash for a simple file
--SKIPIF--
<?php
if (!function_exists('md5_file') || !function_exists('file_put_contents') || !function_exists('tempnam')) {
    echo 'skip: file functions not available';
}
?>
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'phl_md5_');
file_put_contents($fname, "hello world");
echo md5_file($fname) . "\n";
unlink($fname);
?>
--EXPECT--
5eb63bbbe01eeed093cb22bb8f5acdc3
