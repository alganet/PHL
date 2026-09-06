--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ASSERT_* constants have correct PHP values

--FILE--
<?php
error_reporting(E_ALL & ~E_DEPRECATED);
echo "ASSERT_ACTIVE=" . ASSERT_ACTIVE . "\n";
echo "ASSERT_CALLBACK=" . ASSERT_CALLBACK . "\n";
echo "ASSERT_BAIL=" . ASSERT_BAIL . "\n";
echo "ASSERT_WARNING=" . ASSERT_WARNING . "\n";
echo "ASSERT_EXCEPTION=" . ASSERT_EXCEPTION . "\n";
?>
--EXPECTF--
%AASSERT_ACTIVE=1%AASSERT_CALLBACK=2%AASSERT_BAIL=3%AASSERT_WARNING=4%AASSERT_EXCEPTION=5%A
--CLEAN--
<?php

