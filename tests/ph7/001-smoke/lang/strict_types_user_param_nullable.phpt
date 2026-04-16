--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types still accepts null for nullable parameters
--FILE--
<?php
declare(strict_types=1);
function st_p_nullable(?int $x): string { return $x === null ? "null" : "int:$x"; }
echo st_p_nullable(null), "\n";
echo st_p_nullable(3), "\n";
?>
--EXPECT--
null
int:3
