--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: empty body, no properties
--FILE--
<?php
$o = new class {};
var_export(get_object_vars($o));
echo "\n", substr(get_class($o), 0, 15), "\n";
?>
--EXPECT--
array (
)
class@anonymous
--CLEAN--
<?php
unset($o);
