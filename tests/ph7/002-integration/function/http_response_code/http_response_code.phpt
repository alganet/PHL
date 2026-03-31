--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http_response_code() in CLI mode returns false
--FILE--
<?php
echo http_response_code() === false ? "false" : "unexpected";
?>
--EXPECT--
false
