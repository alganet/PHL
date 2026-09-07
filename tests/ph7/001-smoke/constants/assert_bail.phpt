--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ASSERT_BAIL constant
--FILE--
<?php
echo "ASSERT_BAIL=" . ASSERT_BAIL . "\n";
?>
--EXPECTF--
%AConstant ASSERT_BAIL is deprecated since 8.3, as assert_options() is deprecated%AASSERT_BAIL=3%A
--CLEAN--
<?php

