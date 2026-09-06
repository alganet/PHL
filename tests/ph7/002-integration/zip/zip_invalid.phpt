--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
zip_open returns false for non-existent archive
--FILE--
<?php
$fn = sys_get_temp_dir() . '/ph7_zip_nonexistent_8a1f9b.zip';
$res = @zip_open($fn);
echo (is_resource($res) ? 'open_ok' : 'open_failed') . PHP_EOL;
?>
--EXPECTF--
%Aopen_failed%A
--CLEAN--
<?php
unset($fn, $res);
