--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SPL exceptions: instanceof across the hierarchy
--FILE--
<?php
// Inline checks (no global helper) to avoid in-process name clashes.
echo (new InvalidArgumentException() instanceof LogicException) ? "yes\n" : "no\n";
echo (new InvalidArgumentException() instanceof Exception) ? "yes\n" : "no\n";
echo (new InvalidArgumentException() instanceof Throwable) ? "yes\n" : "no\n";
echo (new BadMethodCallException() instanceof BadFunctionCallException) ? "yes\n" : "no\n";
echo (new BadMethodCallException() instanceof LogicException) ? "yes\n" : "no\n";
echo (new RangeException() instanceof RuntimeException) ? "yes\n" : "no\n";
echo (new RangeException() instanceof Exception) ? "yes\n" : "no\n";
// OutOfBoundsException is a RuntimeException in PHP, NOT a LogicException
echo (new OutOfBoundsException() instanceof RuntimeException) ? "yes\n" : "no\n";
echo (new OutOfBoundsException() instanceof LogicException) ? "yes\n" : "no\n";
?>
--EXPECT--
yes
yes
yes
yes
yes
yes
yes
yes
no
--CLEAN--
<?php
