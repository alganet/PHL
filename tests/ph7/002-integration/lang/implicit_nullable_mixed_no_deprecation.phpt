--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mixed $x = null does NOT trigger the php 8.4 implicit-nullable deprecation
--DESCRIPTION--
Regression: PHL emitted the implicit-nullable deprecation for `mixed $x = null`,
but php does not — `mixed` already includes null, so the default is not
"implicitly" marking the parameter nullable. Exact --EXPECT-- (no %A): a spurious
"Deprecated:" line printed to stderr would break the match. The int case (which
DOES deprecate) is covered by implicit_nullable_deprecation.phpt.
--FILE--
<?php
function mf(mixed $m = null) { return $m; }
echo mf() === null ? "mixed-ok" : "bad", "\n";
echo "done\n";
?>
--EXPECT--
mixed-ok
done
--CLEAN--
<?php
