--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chdir() coerces a scalar argument to string and warns when it does not exist
--FILE--
<?php
// php coerces 123 to "123", attempts it, and warns -- it does not silently
// return false because the argument was not already a string.
var_dump(chdir(123));
?>
--EXPECTF--
%Achdir(): No such file or directory (errno 2) in %s on line %d
bool(false)
--CLEAN--
<?php
