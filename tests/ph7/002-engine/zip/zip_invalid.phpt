--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
zip_open returns false for non-existent archive
--SKIPIF--
<?php
/* Only run under PH7/PHL - Zend prints different error formats */
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
if (!function_exists('zip_open')) { echo 'skip: zip not available'; }
?>
--FILE--
<?php
$fn = sys_get_temp_dir() . '/ph7_zip_nonexistent_8a1f9b.zip';
$res = @zip_open($fn);
echo (is_resource($res) ? 'open_ok' : 'open_failed') . PHP_EOL;
?>
--EXPECTF--
%s Error: zip_open(): IO error while opening %s
open_failed

