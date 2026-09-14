--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
highlight.* ini directives: php defaults and set/get round-trip
--FILE--
<?php
foreach (['highlight.comment','highlight.default','highlight.html','highlight.keyword','highlight.string'] as $k) {
    echo $k, '=', ini_get($k), "\n";
}
$old = ini_set('highlight.keyword', '#123456');
echo 'old=', $old, ' new=', ini_get('highlight.keyword'), "\n";
var_dump(ini_get('highlight.nonexistent'));
--EXPECT--
highlight.comment=#FF8000
highlight.default=#0000BB
highlight.html=#000000
highlight.keyword=#007700
highlight.string=#DD0000
old=#007700 new=#123456
bool(false)
