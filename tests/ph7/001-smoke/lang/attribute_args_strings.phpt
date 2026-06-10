--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Brackets and escapes inside attribute argument strings do not unbalance the group
--FILE--
<?php
#[Route("/a]b", 'c[d')] function attr_args_str_f(){ return "both quotes"; }
echo attr_args_str_f(), "\n";
#[Pattern("esc\"quote]still[inside")] function attr_args_esc_f(){ return "escaped"; }
echo attr_args_esc_f(), "\n";
?>
--EXPECT--
both quotes
escaped
--CLEAN--
<?php
?>
