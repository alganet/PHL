--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert_options set ASSERT_BAIL returns old value and changes setting
--FILE--
<?php
error_reporting(E_ALL & ~E_DEPRECATED);
$old = assert_options(ASSERT_BAIL, 1);
echo "old=$old\n";
echo "now=" . assert_options(ASSERT_BAIL) . "\n";
assert_options(ASSERT_BAIL, 0);
echo "restored=" . assert_options(ASSERT_BAIL) . "\n";
?>
--EXPECT--
old=0
now=1
restored=0
--CLEAN--
<?php
unset($old);
