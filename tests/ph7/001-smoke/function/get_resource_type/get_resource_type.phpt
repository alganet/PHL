--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_resource_type returns "stream" for an IO handle
--FILE--
<?php
$f = fopen(__FILE__, 'r');
var_dump(get_resource_type($f));
fclose($f);
$d = opendir(__DIR__);
var_dump(get_resource_type($d));
closedir($d);
?>
--EXPECT--
string(6) "stream"
string(6) "stream"
--CLEAN--
<?php
