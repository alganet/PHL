--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: LOCK_EX constant
--FILE--
<?php
echo "LOCK_EX=" . LOCK_EX . "\n";
?>
--EXPECTF--
LOCK_EX=%d

