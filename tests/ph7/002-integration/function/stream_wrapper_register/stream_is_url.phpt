--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stream_wrapper_register()'s $flags: STREAM_IS_URL marks a wrapper the configuration can switch off, and allow_url_include is off by default
--FILE--
<?php
// stream_wrapper_register()'s $flags has exactly one bit, STREAM_IS_URL, and it
// is the whole reason the argument exists: a wrapper that says it speaks to the
// NETWORK is the one allow_url_fopen and allow_url_include turn off. The constant
// was undefined and the argument was read by nothing, so the call was a fatal
// spelled the documented way and, spelled with a literal 1, registered a wrapper
// that the configuration could no longer switch off -- including for `include`,
// which EXECUTES what comes back.
var_dump(STREAM_IS_URL);

class SwxW
{
    public $context;
    private $done = false;
    public function stream_open($swx_p, $swx_m, $swx_o, &$swx_open) { $this->done = false; return true; }
    public function stream_read($swx_n) { if ($this->done) { return ''; } $this->done = true; return '<?php $GLOBALS["SWX"] = 1; ?>'; }
    public function stream_eof() { return $this->done; }
    public function stream_stat() { return []; }
    public function url_stat($swx_p, $swx_f) { return []; }
}
var_dump(stream_wrapper_register('swxplain', 'SwxW'));
var_dump(stream_wrapper_register('swxurl', 'SwxW', STREAM_IS_URL));

$swx_err = [];
set_error_handler(function ($swx_n, $swx_m) use (&$swx_err) {
    // Only the gate's own message: the generic "could not open" wording that
    // follows it is each engine's, and php adds a stream_set_option notice.
    if (strpos($swx_m, 'wrapper is disabled') !== false) { $swx_err[] = $swx_m; }
    return true;
});

// allow_url_fopen is ON here, so both wrappers open.
foreach (['swxplain', 'swxurl'] as $swx_s) {
    $swx_f = fopen("$swx_s://x", 'r');
    echo "$swx_s fopen=", var_export(is_resource($swx_f), true);
    if ($swx_f) { fclose($swx_f); }
    echo ' fgc=', var_export(file_get_contents("$swx_s://y") !== false, true), "\n";
}

// allow_url_include is OFF by default in php, so the URL wrapper may be READ and
// not INCLUDED -- which is the classic remote-file-inclusion door.
foreach (['swxplain', 'swxurl'] as $swx_s) {
    $GLOBALS['SWX'] = 0;
    $swx_r = include "$swx_s://y";
    echo "$swx_s include failed=", var_export($swx_r === false, true), " ran=", $GLOBALS['SWX'], "\n";
}
// php marks its own data:// wrapper a URL, and that is the one that matters:
// `include 'data://…'` executes bytes carried in the URI itself, which is why
// php refuses it under the default allow_url_include=0. It ran here.
$GLOBALS['SWX'] = 0;
$swx_r = include 'data://text/plain,<?php $GLOBALS["SWX"] = 1; ?>';
echo 'data:// include failed=', var_export($swx_r === false, true), ' ran=', $GLOBALS['SWX'], "\n";
// Reading one is not executing one, and reading stays allowed.
var_dump(file_get_contents('data://text/plain,hello'));
// php:// is NOT a URL wrapper in php and is never gated.
var_dump(file_get_contents('php://memory'));

restore_error_handler();
print_r($swx_err);
?>
--EXPECT--
int(1)
bool(true)
bool(true)
swxplain fopen=true fgc=true
swxurl fopen=true fgc=true
swxplain include failed=false ran=1
swxurl include failed=true ran=0
data:// include failed=true ran=0
string(5) "hello"
string(0) ""
Array
(
    [0] => include(): swxurl:// wrapper is disabled in the server configuration by allow_url_include=0
    [1] => include(): data:// wrapper is disabled in the server configuration by allow_url_include=0
)
