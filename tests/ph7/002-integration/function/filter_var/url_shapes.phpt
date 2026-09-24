--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_URL: the host-optional schemes, the http host rule, PATH_REQUIRED and QUERY_REQUIRED
--FILE--
<?php
/* php's URL filter is a sequence: run the SANITIZE_URL map and refuse the value
 * if a byte was dropped, parse it, require a SCHEME, require a HOST for every
 * scheme except mailto/news/file, hold an http(s) host to the hostname rule,
 * and test the two REQUIRED flags. PHL required a scheme and a host and stopped
 * there — so `mailto:a@b.c` was not a URL, `http://x_y.com/` was, and both
 * flags were undefined constants. */
function ur($u)
{
    $f = [0, FILTER_FLAG_PATH_REQUIRED, FILTER_FLAG_QUERY_REQUIRED,
          FILTER_FLAG_PATH_REQUIRED | FILTER_FLAG_QUERY_REQUIRED];
    $out = '';
    foreach ($f as $flags) {
        $out .= (filter_var($u, FILTER_VALIDATE_URL, $flags) === false ? 'F' : 'ok') . ' ';
    }
    printf("%-32s none/path/query/both = %s\n", '[' . addcslashes($u, "\0..\37") . ']', $out);
}
foreach (['http://x.com', 'http://x.com/', 'http://x.com/p', 'http://x.com/?q=1',
          'http://x.com?q=1', 'http://x.com/p?q#f', 'https://u:p@x.com:8080/a/b?c=d#e',
          'mailto:a@b.c', 'news:comp.lang', 'file:///etc/passwd', 'file://x/etc',
          'tel:+123', 'urn:isbn:123', 'javascript:alert(1)', 'data:text/plain,a',
          'scheme://x', 'x://y', 'ldap://ds.example.com/dc=e',
          'http://', 'http:///p', 'http://:80/', '//x.com/p', '/p/a', 'x.com',
          'http://x.com:80', 'http://x.com:abc', 'http://[::1]:8080/', 'http://[::1',
          'http://1.2.3.4/', 'http://x_y.com/', 'x-y://x_y.com/', 'http://x y.com/',
          'http://x.com/a b', 'http://x.com/a%20b', "http://x.com/\n",
          'HTTP://X.COM', 'http://u!:p@x.com/', 'http://u p@x.com/'] as $u) {
    ur($u);
}
?>
--EXPECT--
[http://x.com]                   none/path/query/both = ok F F F 
[http://x.com/]                  none/path/query/both = ok ok F F 
[http://x.com/p]                 none/path/query/both = ok ok F F 
[http://x.com/?q=1]              none/path/query/both = ok ok ok ok 
[http://x.com?q=1]               none/path/query/both = ok F ok F 
[http://x.com/p?q#f]             none/path/query/both = ok ok ok ok 
[https://u:p@x.com:8080/a/b?c=d#e] none/path/query/both = ok ok ok ok 
[mailto:a@b.c]                   none/path/query/both = ok ok F F 
[news:comp.lang]                 none/path/query/both = ok ok F F 
[file:///etc/passwd]             none/path/query/both = ok ok F F 
[file://x/etc]                   none/path/query/both = ok ok F F 
[tel:+123]                       none/path/query/both = F F F F 
[urn:isbn:123]                   none/path/query/both = F F F F 
[javascript:alert(1)]            none/path/query/both = F F F F 
[data:text/plain,a]              none/path/query/both = F F F F 
[scheme://x]                     none/path/query/both = ok F F F 
[x://y]                          none/path/query/both = ok F F F 
[ldap://ds.example.com/dc=e]     none/path/query/both = ok ok F F 
[http://]                        none/path/query/both = F F F F 
[http:///p]                      none/path/query/both = F F F F 
[http://:80/]                    none/path/query/both = F F F F 
[//x.com/p]                      none/path/query/both = F F F F 
[/p/a]                           none/path/query/both = F F F F 
[x.com]                          none/path/query/both = F F F F 
[http://x.com:80]                none/path/query/both = ok F F F 
[http://x.com:abc]               none/path/query/both = F F F F 
[http://[::1]:8080/]             none/path/query/both = ok ok F F 
[http://[::1]                    none/path/query/both = F F F F 
[http://1.2.3.4/]                none/path/query/both = ok ok F F 
[http://x_y.com/]                none/path/query/both = F F F F 
[x-y://x_y.com/]                 none/path/query/both = ok ok F F 
[http://x y.com/]                none/path/query/both = F F F F 
[http://x.com/a b]               none/path/query/both = F F F F 
[http://x.com/a%20b]             none/path/query/both = ok ok F F 
[http://x.com/\n]                none/path/query/both = F F F F 
[HTTP://X.COM]                   none/path/query/both = ok F F F 
[http://u!:p@x.com/]             none/path/query/both = ok ok F F 
[http://u p@x.com/]              none/path/query/both = F F F F 
--CLEAN--
<?php
