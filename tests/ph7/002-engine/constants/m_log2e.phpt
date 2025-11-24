--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: M_LOG2E math constant
--FILE--
<?php
echo "M_LOG2E=" . sprintf('%.8f', M_LOG2E) . "\n";
?>
--EXPECTF--
M_LOG2E=1.44%d

