--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 heredoc as function argument followed by comma
--FILE--
<?php
function join3($a, $b, $c) {
    return $a . '|' . $b . '|' . $c;
}

echo join3(<<<EOT
    first
    EOT, "second", "third") . "\n";

echo join3("alpha", <<<EOT
    beta
    EOT, "gamma") . "\n";
--EXPECT--
first|second|third
alpha|beta|gamma
--CLEAN--
<?php

