--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DEBUG_BACKTRACE_PROVIDE_OBJECT constant
--FILE--
<?php
echo "DEBUG_BACKTRACE_PROVIDE_OBJECT=" . DEBUG_BACKTRACE_PROVIDE_OBJECT . "\n";
?>
--EXPECTF--
DEBUG_BACKTRACE_PROVIDE_OBJECT=%d
