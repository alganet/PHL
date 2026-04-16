--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Without strict_types, scalar args and returns still coerce weakly
--FILE--
<?php
// No declare(strict_types=1) — weak mode is the default.
function st_off_coerce(int $x): int { return $x; }
// Numeric string -> int without strict_types is silent.
echo st_off_coerce("42"), "\n";
// Booleans are always implicitly int-compatible in weak mode.
echo st_off_coerce(true), "\n";
?>
--EXPECT--
42
1
