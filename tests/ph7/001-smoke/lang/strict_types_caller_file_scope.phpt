--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types is per-file: a weak-mode caller of a strict-defined function still coerces args
--FILE--
<?php
// Main file is weak-mode.
include __DIR__ . '/strict_types_caller_file_scope.inc';
// takes(int) was defined inside the strict-mode include. Calling it from
// this weak-mode file should allow argument coercion ("5" -> 5), because
// PHP scopes strict_types to the caller's file.
echo takes("5"), "\n";
?>
--EXPECT--
5
