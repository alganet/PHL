--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func_array simple function
--FILE--
<?php
function cufatriple($x){ return $x * 3; }
echo call_user_func_array('cufatriple', array(3)) . "\n";
?>
--EXPECT--
9
