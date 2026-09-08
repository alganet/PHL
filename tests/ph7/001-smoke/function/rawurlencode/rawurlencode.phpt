--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rawurlencode uses %20 and keeps ~; both encoders encode $; empty input is ""
--FILE--
<?php
echo urlencode("a b"), "|", rawurlencode("a b"), "\n";
echo urlencode("~"), "|", rawurlencode("~"), "\n";
echo urlencode('a$b'), "|", rawurlencode('a$b'), "\n";
echo "[", urlencode(""), "][", rawurlencode(""), "][", urldecode(""), "]\n";
echo rawurldecode("a%20b"), "|", urldecode("a+b"), "\n";
echo rawurlencode("hello world!"), "\n";
?>
--EXPECT--
a+b|a%20b
%7E|~
a%24b|a%24b
[][][]
a b|a b
hello%20world%21
--CLEAN--
<?php
