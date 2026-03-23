--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert_options set ASSERT_ACTIVE returns old value and changes setting
--FILE--
<?php
error_reporting(E_ALL & ~E_DEPRECATED);
$old = assert_options(ASSERT_ACTIVE, 0);
echo "old=$old\n";
echo "now=" . assert_options(ASSERT_ACTIVE) . "\n";
assert_options(ASSERT_ACTIVE, 1);
echo "restored=" . assert_options(ASSERT_ACTIVE) . "\n";
?>
--EXPECT--
old=1
now=0
restored=1
--CLEAN--
<?php
unset($old);
