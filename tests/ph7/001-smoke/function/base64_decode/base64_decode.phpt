--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base64_decode decodes a base64 string
--FILE--
<?php
echo base64_decode('YQ==') . "\n";
?>
--EXPECT--
a
--CLEAN--
<?php

