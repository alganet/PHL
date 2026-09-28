--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_extensions answers the list in force after the call
--FILE--
<?php
var_dump(spl_autoload_extensions());
var_dump(spl_autoload_extensions('.foo,.bar'));
var_dump(spl_autoload_extensions());
var_dump(spl_autoload_extensions(null));
var_dump(spl_autoload_extensions(false));
var_dump(spl_autoload_extensions(null));
spl_autoload_extensions('.inc,.php');
var_dump(spl_autoload_extensions());
?>
--EXPECT--
string(9) ".inc,.php"
string(9) ".foo,.bar"
string(9) ".foo,.bar"
string(9) ".foo,.bar"
string(0) ""
string(0) ""
string(9) ".inc,.php"
--CLEAN--
<?php
