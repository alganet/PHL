--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ASSERT_WARNING constant
--FILE--
<?php
echo "ASSERT_WARNING=" . ASSERT_WARNING . "\n";
?>
--EXPECTF--
%AConstant ASSERT_WARNING is deprecated since 8.3, as assert_options() is deprecated%AASSERT_WARNING=4%A
--CLEAN--
<?php

