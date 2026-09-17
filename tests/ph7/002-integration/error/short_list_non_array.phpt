--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Symmetric array destructuring from non-array source emits warning

--FILE--
<?php
[$a] = "string";
echo isset($a) ? "set" : "not set";
?>
--EXPECTF--
%AWarning:%ACannot use string as array in %s on line %d%A
--CLEAN--
<?php
unset($a);
