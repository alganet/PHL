--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a const-list compile error blames the `const` statement's line, not the element's
--FILE--
<?php
// The offending initializer (a call, forbidden in a constant expression) sits on
// the line AFTER the `const` keyword. php attributes the fatal to the keyword's
// line (5), not the call's own line (6); PHL now matches.
const OK = 1,
      BAD = strlen("x");
echo OK, "\n";
?>
--EXPECTF--
%AConstant expression contains invalid operations in %s on line 5%A
--CLEAN--
<?php

