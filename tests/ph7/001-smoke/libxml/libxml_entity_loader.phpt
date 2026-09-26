--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
libxml_set/get_external_entity_loader() round-trip the resolver verbatim and libxml_set_streams_context() takes only a real stream-context, php's refusals included
--FILE--
<?php
// The loader slot: unset answers null, a callable round-trips IDENTICALLY,
// null clears, and a non-callable is php's callback TypeError. Nothing in a
// CLI parse without external loading ever invokes it -- php's sanitized
// parser options keep external entities off -- so the slot's store/answer
// contract is the whole observable surface here.
var_dump(libxml_get_external_entity_loader());
$xll1 = function ($public, $system, $context) { return null; };
var_dump(libxml_set_external_entity_loader($xll1));
var_dump(libxml_get_external_entity_loader() === $xll1);
var_dump(libxml_set_external_entity_loader('trim'));
var_dump(libxml_get_external_entity_loader());
var_dump(libxml_set_external_entity_loader(null));
var_dump(libxml_get_external_entity_loader());
try {
    libxml_set_external_entity_loader("no_such_fn");
} catch (TypeError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
try {
    libxml_set_external_entity_loader(42);
} catch (TypeError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
// The streams-context slot takes a stream-context resource and nothing else.
$xllCtx = stream_context_create(['http' => ['user_agent' => 'phl']]);
var_dump(libxml_set_streams_context($xllCtx));
try {
    libxml_set_streams_context(42);
} catch (TypeError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
$xllFh = fopen('php://memory', 'r');
try {
    libxml_set_streams_context($xllFh);
} catch (TypeError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
fclose($xllFh);
?>
--EXPECT--
NULL
bool(true)
bool(true)
bool(true)
string(4) "trim"
bool(true)
NULL
TypeError: libxml_set_external_entity_loader(): Argument #1 ($resolver_function) must be a valid callback or null, function "no_such_fn" not found or invalid function name
TypeError: libxml_set_external_entity_loader(): Argument #1 ($resolver_function) must be a valid callback or null, no array or string given
NULL
TypeError: libxml_set_streams_context(): Argument #1 ($context) must be of type resource, int given
TypeError: libxml_set_streams_context(): supplied resource is not a valid Stream-Context resource
