--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types accepts any exact member of a union parameter
--FILE--
<?php
declare(strict_types=1);
function st_p_union(int|string $x): string {
    return is_int($x) ? "int:$x" : "string:$x";
}
echo st_p_union(1), "\n";
echo st_p_union("a"), "\n";
?>
--EXPECT--
int:1
string:a
