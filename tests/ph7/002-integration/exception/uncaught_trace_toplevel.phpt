--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Uncaught report at file scope prints "#0 {main}" only
--DESCRIPTION--
Regression: the uncaught renderer synthesized its own trace and invented a
"#0 file(line): {main}" frame here -- {main} is php's bottom marker, not a
called frame, so a throw at file scope has exactly one trace line.
--FILE--
<?php
throw new Exception('at top');
?>
--EXPECTF--
%APHP Fatal error:  Uncaught Exception: at top in %s:2
Stack trace:
#0 {main}
  thrown in %s on line 2%A
