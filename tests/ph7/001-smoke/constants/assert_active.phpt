--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ASSERT_ACTIVE constant
--FILE--
<?php
echo "ASSERT_ACTIVE=" . ASSERT_ACTIVE . "\n";
?>
--EXPECTF--
%AConstant ASSERT_ACTIVE is deprecated since 8.3, as assert_options() is deprecated%AASSERT_ACTIVE=1%A
--CLEAN--
<?php

