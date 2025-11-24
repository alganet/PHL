--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_resource(fopen) returns true
--FILE--
<?php
$h = fopen(__FILE__, 'r');
echo "is_resource_true=" . (is_resource($h) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_resource_true=true
--CLEAN--
<?php
fclose($h);
unset($h);
?>
