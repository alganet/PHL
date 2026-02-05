--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_url function basic tests
--SKIPIF--
<?php if (!function_exists('parse_url')) { die('skip'); } ?>
--FILE--
<?php
// Test basic URL parsing
$url = "http://example.com:8080/path?query=value#fragment";
$result = parse_url($url);
echo isset($result['scheme']) && $result['scheme'] == 'http' ? "scheme_ok\n" : "scheme_fail\n";
echo isset($result['host']) && $result['host'] == 'example.com' ? "host_ok\n" : "host_fail\n";
echo isset($result['port']) && $result['port'] == 8080 ? "port_ok\n" : "port_fail\n";
echo isset($result['path']) && $result['path'] == '/path' ? "path_ok\n" : "path_fail\n";
echo isset($result['query']) && $result['query'] == 'query=value' ? "query_ok\n" : "query_fail\n";
echo isset($result['fragment']) && $result['fragment'] == 'fragment' ? "fragment_ok\n" : "fragment_fail\n";

// Test URL without port
$url2 = "https://test.com/test";
$result2 = parse_url($url2);
echo isset($result2['scheme']) && $result2['scheme'] == 'https' ? "scheme2_ok\n" : "scheme2_fail\n";
echo isset($result2['host']) && $result2['host'] == 'test.com' ? "host2_ok\n" : "host2_fail\n";


?>
--EXPECT--
scheme_ok
host_ok
port_ok
path_ok
query_ok
fragment_ok
scheme2_ok
host2_ok
--CLEAN--
<?php
unset($url, $result, $url2, $result2);
