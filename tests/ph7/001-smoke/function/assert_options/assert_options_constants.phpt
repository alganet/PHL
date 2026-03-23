--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ASSERT_* constants have correct PHP values
--SKIPIF--
<?php if (function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
error_reporting(E_ALL & ~E_DEPRECATED);
echo "ASSERT_ACTIVE=" . ASSERT_ACTIVE . "\n";
echo "ASSERT_CALLBACK=" . ASSERT_CALLBACK . "\n";
echo "ASSERT_BAIL=" . ASSERT_BAIL . "\n";
echo "ASSERT_WARNING=" . ASSERT_WARNING . "\n";
echo "ASSERT_EXCEPTION=" . ASSERT_EXCEPTION . "\n";
?>
--EXPECT--
ASSERT_ACTIVE=1
ASSERT_CALLBACK=2
ASSERT_BAIL=3
ASSERT_WARNING=4
ASSERT_EXCEPTION=5
--CLEAN--
<?php

