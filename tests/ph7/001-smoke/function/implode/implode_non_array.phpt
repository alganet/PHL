--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode with non-array argument treats as string
--FILE--
<?php
$result = implode(",", array("not array"));
echo $result . "\n";
?>
--EXPECT--
not array
--CLEAN--
<?php
unset($result);
