--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: addcslashes with empty string returns empty string
--FILE--
<?php
$result = addcslashes('', 'a');
echo $result . "\n";
?>
--EXPECT--
--CLEAN--
<?php
unset($result);
