--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
"return null;" from a void function carries php's "did you mean return;" hint
--FILE--
<?php
function bad(): void { return NULL; }
?>
--EXPECTF--
%s Fatal error:  A void function must not return a value (did you mean "return;" instead of "return null;"?) in %s
--CLEAN--
<?php
