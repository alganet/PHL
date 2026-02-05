--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_url handles edge cases and special URLs
--SKIPIF--
<?php if (!function_exists('parse_url')) { die('skip'); } ?>
--FILE--
<?php
// Test URLs with special characters and edge cases
$urls = array(
    "ftp://user:pass@example.com:21/path",  // FTP with auth
    "file:///etc/passwd",                   // File URL
    "mailto:user@example.com",              // Mailto
    "http://example.com:80",                // Explicit port 80
    "https://example.com:443",              // Explicit port 443
    "http://[::1]/path",                    // IPv6
    "http://example.com/path with spaces",  // Spaces in path
    "//example.com/path",                   // Protocol-relative
    "?query=value",                         // Query only
    "#fragment",                            // Fragment only
);

foreach ($urls as $i => $url) {
    $result = parse_url($url);
    echo "url" . $i . "_parsed: " . (is_array($result) ? "ok" : "fail") . "\n";
}
?>
--EXPECT--
url0_parsed: ok
url1_parsed: ok
url2_parsed: ok
url3_parsed: ok
url4_parsed: ok
url5_parsed: ok
url6_parsed: ok
url7_parsed: ok
url8_parsed: ok
url9_parsed: ok
--CLEAN--
<?php
unset($urls, $result);
