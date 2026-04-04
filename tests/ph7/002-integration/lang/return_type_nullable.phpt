--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullable return type declarations with ? prefix
--FILE--
<?php
function maybeString(): ?string { return "hello"; }
function maybeNull(): ?string { return null; }
function maybeInt(): ?int { return null; }

echo maybeString() . "\n";
echo is_null(maybeNull()) ? "null" : "not null";
echo "\n";
echo is_null(maybeInt()) ? "null" : "not null";
echo "\n";
?>
--EXPECT--
hello
null
null
--CLEAN--
<?php

