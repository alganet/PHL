--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_quote escapes special regex characters
--FILE--
<?php
echo preg_quote('Hello.World+Foo*Bar') . "\n";
echo preg_quote('test/path', '/') . "\n";
echo preg_quote('$100') . "\n";
?>
--EXPECT--
Hello\.World\+Foo\*Bar
test\/path
\$100
--CLEAN--
<?php

