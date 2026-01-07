--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Heredoc and nowdoc string syntax
--FILE--
<?php
// Test heredoc syntax (<<<)
// Variables are expanded in heredoc

$name = "World";
$age = 25;

$heredoc = <<<EOD
Hello $name!
You are $age years old.
This is a heredoc test.
EOD;

echo $heredoc;
echo "\n";

// Test nowdoc syntax (<<<'EOD')
// Variables are NOT expanded in nowdoc (like single quotes)

$nowdoc = <<<'EOD'
Hello $name!
You are $age years old.
This is a nowdoc test.
EOD;

echo $nowdoc;
echo "\n";

// Test with different delimiters
$custom_heredoc = <<<CUSTOM
Testing custom delimiter: $name
CUSTOM;

echo $custom_heredoc;
echo "\n";

// Test nested heredoc-like content
$nested = <<<NESTED
This contains <<<INSIDE syntax that should not be interpreted
INSIDE
But it's part of the heredoc content.
NESTED;

echo $nested;
?>
--EXPECT--
Hello World!
You are 25 years old.
This is a heredoc test.

Hello $name!
You are $age years old.
This is a nowdoc test.

Testing custom delimiter: World

This contains <<<INSIDE syntax that should not be interpreted
INSIDE
But it's part of the heredoc content.