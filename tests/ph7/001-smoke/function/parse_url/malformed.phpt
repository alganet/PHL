--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_url on relative, malformed and authority-less URLs
--FILE--
<?php
// A relative URL is a PATH; only a "//" introduces an authority.
foreach (["", "/", "a", "a/b", "./b", "?q=1", "#f", "a?q=1#f",
          "mailto:me@x.com", "x:", "x:y", "http:/x",
          "//host", "//host/p", "//u@h", "http://u:p@x:8080/y?q#f",
          "host:81/p", "1:2", "file:///tmp/a",
          "//", "///", "http://", ":80", ":", "http://x:abc/"] as $u) {
    $r = parse_url($u);
    echo str_pad(var_export($u, true), 26), " => ";
    if ($r === false) { echo "false\n"; continue; }
    $o = [];
    foreach ($r as $k => $v) $o[] = "$k=" . var_export($v, true);
    echo "[", implode(", ", $o), "]\n";
}
// A control byte never survives into a component.
var_dump(parse_url("/a\tb")["path"]);
// Component selection, and the ValueError for an id that is not one.
var_dump(parse_url("http://h:8/p?q#f", PHP_URL_HOST));
var_dump(parse_url("http://h:8/p?q#f", PHP_URL_PORT));
var_dump(parse_url("http://h:8/p?q#f", PHP_URL_FRAGMENT));
var_dump(parse_url("http://h/p", PHP_URL_QUERY));
try { parse_url("http://h/p", 8); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
''                         => [path='']
'/'                        => [path='/']
'a'                        => [path='a']
'a/b'                      => [path='a/b']
'./b'                      => [path='./b']
'?q=1'                     => [query='q=1']
'#f'                       => [fragment='f']
'a?q=1#f'                  => [path='a', query='q=1', fragment='f']
'mailto:me@x.com'          => [scheme='mailto', path='me@x.com']
'x:'                       => [scheme='x']
'x:y'                      => [scheme='x', path='y']
'http:/x'                  => [scheme='http', path='/x']
'//host'                   => [host='host']
'//host/p'                 => [host='host', path='/p']
'//u@h'                    => [host='h', user='u']
'http://u:p@x:8080/y?q#f'  => [scheme='http', host='x', port=8080, user='u', pass='p', path='/y', query='q', fragment='f']
'host:81/p'                => [host='host', port=81, path='/p']
'1:2'                      => [host='1', port=2]
'file:///tmp/a'            => [scheme='file', path='/tmp/a']
'//'                       => false
'///'                      => false
'http://'                  => false
':80'                      => false
':'                        => false
'http://x:abc/'            => false
string(4) "/a_b"
string(1) "h"
int(8)
string(1) "f"
NULL
parse_url(): Argument #2 ($component) must be a valid URL component identifier, 8 given
--CLEAN--
<?php
