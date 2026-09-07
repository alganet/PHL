--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ASSERT_CALLBACK constant
--FILE--
<?php
echo "ASSERT_CALLBACK=" . ASSERT_CALLBACK . "\n";
?>
--EXPECTF--
%AConstant ASSERT_CALLBACK is deprecated since 8.3, as assert_options() is deprecated%AASSERT_CALLBACK=2%A
--CLEAN--
<?php

