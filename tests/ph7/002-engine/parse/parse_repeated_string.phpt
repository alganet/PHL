--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Repeated string literals
--FILE--
<?php
$a = 'hello';
$b = 'hello';
echo "done";
?>
--EXPECT--
done