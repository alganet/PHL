--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base64_encode encodes a string to base64
--FILE--
<?php
echo base64_encode('a') . "\n";
?>
--EXPECT--
YQ==
--CLEAN--
<?php

