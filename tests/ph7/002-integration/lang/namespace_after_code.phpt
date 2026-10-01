--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A namespace declaration after code is a compile fatal, and nothing runs
--FILE--
<?php
echo "x\n";
namespace Foo;
echo "should not reach here\n";
?>
--EXPECTF--
%s Fatal error:  Namespace declaration statement has to be the very first statement or after any declare call in the script in %s on line 3
Stack trace:
#0 {main}
