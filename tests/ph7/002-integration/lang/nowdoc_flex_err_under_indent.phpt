--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 nowdoc body indented less than closing marker is a parse error
--FILE--
<?php
$x = <<<'EOT'
  too shallow
      EOT;
--EXPECTF--
PHP %s error:  Invalid body indentation level (expecting an indentation level of at least 6) in %s on line %d
--CLEAN--
<?php
