--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: EXTR_OVERWRITE constant
--FILE--
<?php
echo "EXTR_OVERWRITE=" . EXTR_OVERWRITE . "\n";
?>
--EXPECTF--
EXTR_OVERWRITE=%d
