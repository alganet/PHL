--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
allow_url_fopen=0 refuses to OPEN a STREAM_IS_URL wrapper (and data://), and leaves a plain one alone
--INI--
allow_url_fopen=0
--FILE--
<?php
// The same wrapper with allow_url_fopen=0: the URL one may not even be OPENED,
// and the plain one is untouched.
class SwyW
{
    public $context;
    private $done = false;
    public function stream_open($swy_p, $swy_m, $swy_o, &$swy_open) { $this->done = false; return true; }
    public function stream_read($swy_n) { if ($this->done) { return ''; } $this->done = true; return 'hello'; }
    public function stream_eof() { return $this->done; }
    public function stream_stat() { return []; }
    public function url_stat($swy_p, $swy_f) { return []; }
}
stream_wrapper_register('swyplain', 'SwyW');
stream_wrapper_register('swyurl', 'SwyW', STREAM_IS_URL);
$swy_err = [];
set_error_handler(function ($swy_n, $swy_m) use (&$swy_err) {
    if (strpos($swy_m, 'wrapper is disabled') !== false) { $swy_err[] = $swy_m; }
    return true;
});
foreach (['swyplain', 'swyurl'] as $swy_s) {
    $swy_f = fopen("$swy_s://x", 'r');
    echo "$swy_s fopen=", var_export(is_resource($swy_f), true);
    if ($swy_f) { echo ' read=', var_export(fread($swy_f, 10), true); fclose($swy_f); }
    echo ' fgc=', var_export(file_get_contents("$swy_s://y"), true), "\n";
}
// The built-in data:// wrapper is gated by the same setting...
var_dump(file_get_contents('data://text/plain,hello'));
// ...and php:// never is.
var_dump(file_get_contents('php://memory'));
restore_error_handler();
print_r($swy_err);
?>
--EXPECT--
swyplain fopen=true read='hello' fgc='hello'
swyurl fopen=false fgc=false
bool(false)
string(0) ""
Array
(
    [0] => fopen(): swyurl:// wrapper is disabled in the server configuration by allow_url_fopen=0
    [1] => file_get_contents(): swyurl:// wrapper is disabled in the server configuration by allow_url_fopen=0
    [2] => file_get_contents(): data:// wrapper is disabled in the server configuration by allow_url_fopen=0
)
