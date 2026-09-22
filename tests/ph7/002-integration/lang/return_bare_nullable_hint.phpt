--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a return type that ACCEPTS null carries php's "did you mean return null;" hint
--FILE--
<?php
function bad(): ?int { return; }
?>
--EXPECTF--
%s Fatal error:  A function with return type must return a value (did you mean "return null;" instead of "return;"?) in %s
--CLEAN--
<?php
