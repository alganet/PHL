--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ph7_uname returns system information
--SKIPIF--
<?php
if (!function_exists('ph7_uname')) { echo 'skip: ph7_uname not available'; }
?>
--FILE--
<?php
$uname = ph7_uname();
echo "uname_type=" . gettype($uname) . "\n";
?>
--EXPECT--
uname_type=string
--CLEAN--
<?php
unset($uname);
