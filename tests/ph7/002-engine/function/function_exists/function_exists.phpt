--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
function_exists checks if function exists
--SKIPIF--
<?php
if (!function_exists('function_exists')) { echo 'skip: function_exists not available'; }
?>
--FILE--
<?php
$result = function_exists('strlen');
if ($result) { echo "strlen_exists\n"; } else { echo "strlen_missing\n"; }
$result = function_exists('nonexistent_function_12345');
if ($result) { echo "nonexistent_exists\n"; } else { echo "nonexistent_missing\n"; }
?>
--EXPECT--
strlen_exists
nonexistent_missing
