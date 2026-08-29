--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strstr with before_needle parameter

--FILE--
<?php
$result = strstr("hello world", "lo", true);
echo $result . "\n";
?>
--EXPECT--
hel
--CLEAN--
<?php
unset($result);
