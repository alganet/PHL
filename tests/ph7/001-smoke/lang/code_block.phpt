--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Code block processing

--FILE--
<?php
// Test code block processing
{
    $x = 42;
    echo $x;
}
?>
--EXPECT--
42
--CLEAN--
<?php
unset($x);
