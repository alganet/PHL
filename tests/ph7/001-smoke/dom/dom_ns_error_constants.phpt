--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\ namespace constants: the DOMException codes and the html parser's no-default-namespace flag
--FILE--
<?php
$dom_nec_names = [
    'INDEX_SIZE_ERR', 'STRING_SIZE_ERR', 'HIERARCHY_REQUEST_ERR', 'WRONG_DOCUMENT_ERR',
    'INVALID_CHARACTER_ERR', 'NO_DATA_ALLOWED_ERR', 'NO_MODIFICATION_ALLOWED_ERR',
    'NOT_FOUND_ERR', 'NOT_SUPPORTED_ERR', 'INUSE_ATTRIBUTE_ERR', 'INVALID_STATE_ERR',
    'SYNTAX_ERR', 'INVALID_MODIFICATION_ERR', 'NAMESPACE_ERR', 'VALIDATION_ERR',
    'HTML_NO_DEFAULT_NS',
];
foreach ($dom_nec_names as $dom_nec_n) {
    $dom_nec_q = 'Dom\\' . $dom_nec_n;
    echo $dom_nec_q, ' defined=', var_export(defined($dom_nec_q), true),
         ' value=', var_export(defined($dom_nec_q) ? constant($dom_nec_q) : null, true), "\n";
}
/* the two names the 2004 tree has that this one does not re-export */
foreach (['PHP_ERR', 'INVALID_ACCESS_ERR', 'DOMSTRING_SIZE_ERR'] as $dom_nec_n) {
    echo 'absent Dom\\', $dom_nec_n, '=', var_export(!defined('Dom\\' . $dom_nec_n), true), "\n";
}
/* the constant's own name is case-sensitive */
echo 'Dom\\syntax_err=', var_export(defined('Dom\\syntax_err'), true), "\n";
/* they belong to ext/dom, and follow the 2004 names in its own order */
$dom_nec_c = (new ReflectionExtension('dom'))->getConstants();
$dom_nec_k = array_keys($dom_nec_c);
echo 'after DOM_VALIDATION_ERR: ',
     implode(',', array_slice($dom_nec_k, array_search('DOM_VALIDATION_ERR', $dom_nec_k, true) + 1)), "\n";
echo 'grouped under dom=',
     var_export(isset(get_defined_constants(true)['dom']['Dom\\NAMESPACE_ERR']), true), "\n";
--EXPECT--
Dom\INDEX_SIZE_ERR defined=true value=1
Dom\STRING_SIZE_ERR defined=true value=2
Dom\HIERARCHY_REQUEST_ERR defined=true value=3
Dom\WRONG_DOCUMENT_ERR defined=true value=4
Dom\INVALID_CHARACTER_ERR defined=true value=5
Dom\NO_DATA_ALLOWED_ERR defined=true value=6
Dom\NO_MODIFICATION_ALLOWED_ERR defined=true value=7
Dom\NOT_FOUND_ERR defined=true value=8
Dom\NOT_SUPPORTED_ERR defined=true value=9
Dom\INUSE_ATTRIBUTE_ERR defined=true value=10
Dom\INVALID_STATE_ERR defined=true value=11
Dom\SYNTAX_ERR defined=true value=12
Dom\INVALID_MODIFICATION_ERR defined=true value=13
Dom\NAMESPACE_ERR defined=true value=14
Dom\VALIDATION_ERR defined=true value=16
Dom\HTML_NO_DEFAULT_NS defined=true value=2147483648
absent Dom\PHP_ERR=true
absent Dom\INVALID_ACCESS_ERR=true
absent Dom\DOMSTRING_SIZE_ERR=true
Dom\syntax_err=false
after DOM_VALIDATION_ERR: Dom\INDEX_SIZE_ERR,Dom\STRING_SIZE_ERR,Dom\HIERARCHY_REQUEST_ERR,Dom\WRONG_DOCUMENT_ERR,Dom\INVALID_CHARACTER_ERR,Dom\NO_DATA_ALLOWED_ERR,Dom\NO_MODIFICATION_ALLOWED_ERR,Dom\NOT_FOUND_ERR,Dom\NOT_SUPPORTED_ERR,Dom\INUSE_ATTRIBUTE_ERR,Dom\INVALID_STATE_ERR,Dom\SYNTAX_ERR,Dom\INVALID_MODIFICATION_ERR,Dom\NAMESPACE_ERR,Dom\VALIDATION_ERR,Dom\HTML_NO_DEFAULT_NS
grouped under dom=true
