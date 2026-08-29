--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quotemeta with empty string

--FILE--
<?php
$result = quotemeta("");
echo $result . "\n";
?>
--EXPECT--
--CLEAN--
<?php
unset($result);
