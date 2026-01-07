--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto jumps to label
--FILE--
<?php
goto label;
echo "skipped\n";
label:
echo "reached\n";
?>
--EXPECT--
reached