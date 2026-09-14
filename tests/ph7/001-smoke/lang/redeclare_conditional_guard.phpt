--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Conditional class/function declarations hoist without a redeclaration fatal
--FILE--
<?php
if (false) { class GuardCls {} }
class GuardCls { public $ok = 1; }
echo (new GuardCls)->ok, "\n";
if (!function_exists('guard_fn')) { function guard_fn(){ return "a"; } }
function guard_fn(){ return "b"; }
echo guard_fn(), "\n";
if (true) { class CondCls {} }
echo class_exists('CondCls') ? "cond-exists\n" : "no\n";
--EXPECT--
1
b
cond-exists
