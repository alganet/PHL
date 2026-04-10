--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 heredoc as last argument followed by closing parenthesis
--FILE--
<?php
echo strlen(<<<EOT
    measure me
    EOT) . "\n";

echo str_repeat(<<<EOT
    ab
    EOT, 3) . "\n";
--EXPECT--
10
ababab
--CLEAN--
<?php

