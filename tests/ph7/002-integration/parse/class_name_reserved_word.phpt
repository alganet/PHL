--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A word php reserves may not name a class, and the refusal quotes it as written
--DESCRIPTION--
php's scanner hands `void`, `never`, `null`, `false`, `true`, `mixed`, `iterable`,
`self` and their neighbours over as identifiers and its COMPILER refuses them
(zend_is_reserved_class_name). Declaring one used to succeed here. Its own file
because a compile-time fatal ends the process, so one refusal is all a process
can show; the accepting half of the table is in vendor_declaration_shapes.phpt.
--FILE--
<?php
class Iterable {}
echo "unreached\n";
?>
--EXPECTF--
%AFatal error:%ACannot use "Iterable" as a class name as it is reserved%A
