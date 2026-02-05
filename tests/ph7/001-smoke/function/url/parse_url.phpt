--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_url returns components for a URL
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
?>
--FILE--
<?php
$url = 'http://user:pass@example.com:8080/path/to/file.php?arg=1&arg2=2#frag';
$parsed = parse_url($url);
$ok = (
    isset($parsed['scheme']) && $parsed['scheme'] === 'http' &&
    isset($parsed['host']) && $parsed['host'] === 'example.com' &&
    isset($parsed['path']) && $parsed['path'] === '/path/to/file.php' &&
    isset($parsed['query']) && strpos($parsed['query'], 'arg=1') !== false &&
    isset($parsed['fragment']) && $parsed['fragment'] === 'frag'
) ? "ok" : "fail";
echo $ok . "\n";
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($url, $parsed, $ok);
