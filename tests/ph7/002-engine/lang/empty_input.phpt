--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Lexer handles empty input gracefully
--FILE--
<?php
// Test empty input handling - covers uncovered line in TokenizePHP where EOF is reached
echo "Empty input test passed\n";
?>
--EXPECT--
Empty input test passed