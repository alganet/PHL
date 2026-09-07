--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: E_STRICT constant
--FILE--
<?php
echo "E_STRICT=" . E_STRICT . "\n";
?>
--EXPECTF--
%AConstant E_STRICT is deprecated since 8.4, the error level was removed%AE_STRICT=2048%A
--CLEAN--
<?php

