--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union type parameter: int|null behaves like ?int (accepts null and ints)
--FILE--
<?php
function upnse_show($x) { echo is_null($x) ? "null" : "int:$x", "\n"; }
function upnse_f(int|null $x) { upnse_show($x); }
function upnse_g(?int $x)    { upnse_show($x); }
upnse_f(null); upnse_f(7);
upnse_g(null); upnse_g(7);
?>
--EXPECT--
null
int:7
null
int:7
--CLEAN--
<?php
