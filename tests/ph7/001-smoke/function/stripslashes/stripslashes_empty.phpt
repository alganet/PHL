--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stripslashes with empty string returns an empty string, not NULL
--FILE--
<?php
var_dump(stripslashes(''));
var_dump(stripslashes('\\'));
?>
--EXPECT--
string(0) ""
string(0) ""
--CLEAN--
<?php
