--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in Exception/Error classes have __toString and are auto-implemented as Stringable
--FILE--
<?php
echo "Exception: ", (new Exception("x")) instanceof Stringable ? "yes" : "no", "\n";
echo "Error: ", (new Error("x")) instanceof Stringable ? "yes" : "no", "\n";
echo "TypeError: ", (new TypeError("x")) instanceof Stringable ? "yes" : "no", "\n";
echo "ValueError: ", (new ValueError("x")) instanceof Stringable ? "yes" : "no", "\n";
?>
--EXPECT--
Exception: yes
Error: yes
TypeError: yes
ValueError: yes
--CLEAN--
<?php
