--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Auto-index after a negative first key: PHL continues from 0 (php 8.3+ continues from key+1 — divergence, see array_negative_key_autoindex_zend.phpt)
--FILE--
<?php
$a = [];
$a[-5] = 'a';
$a[] = 'b';
$a[] = 'c';
var_export(array_keys($a));
?>
--EXPECTF--
%Aarray (%A0 => -5,%A1 => -4,%A2 => -3,%A)%A
--CLEAN--
<?php
unset($a);
