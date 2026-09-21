--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
settype() cannot convert to the resource type (ValueError)
--FILE--
<?php
$x = 1;
settype($x, "resource");
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: Cannot convert to resource type in %s
