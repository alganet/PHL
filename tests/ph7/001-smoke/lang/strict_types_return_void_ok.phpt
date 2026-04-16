--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Void return type accepts bare `return;` and end-of-body fall-through
--FILE--
<?php
function st_void_a(): void { return; }
function st_void_b(): void { /* fall-through */ }
st_void_a();
st_void_b();
echo "ok\n";
?>
--EXPECT--
ok
