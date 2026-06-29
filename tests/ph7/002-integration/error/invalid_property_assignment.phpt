--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Assigning an undefined property on a stdClass creates a dynamic property
--FILE--
<?php
$obj = new stdClass();
$obj->nonexistent = "value";
echo $obj->nonexistent, "\n";
echo json_encode($obj), "\n";
?>
--EXPECT--
value
{"nonexistent":"value"}
--CLEAN--
<?php
unset($obj);
