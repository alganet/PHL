--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: instanceof across the hierarchy
--FILE--
<?php
function thb($v) { echo $v ? "yes\n" : "no\n"; }
thb(new Exception() instanceof Throwable);
thb(new Error() instanceof Throwable);
thb(new TypeError() instanceof Throwable);
thb(new ArgumentCountError() instanceof Throwable);
thb(new ValueError() instanceof Throwable);
thb(new ArithmeticError() instanceof Throwable);
thb(new DivisionByZeroError() instanceof Throwable);
thb(new AssertionError() instanceof Throwable);
thb(new ErrorException() instanceof Throwable);
thb(new TypeError() instanceof Error);
thb(new ArgumentCountError() instanceof TypeError);
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
yes
yes
yes
--CLEAN--
<?php
