--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: LOCK_SH constant
--FILE--
<?php
echo "LOCK_SH=" . LOCK_SH . "\n";
?>
--EXPECTF--
LOCK_SH=%d
