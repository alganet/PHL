--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_input: INPUT_* constants, not-set vs present, default/NULL_ON_FAILURE
--FILE--
<?php
if(!function_exists("fi_show")){function fi_show($v){echo var_export($v,true),"\n";}}

/* INPUT_* constant values (php 8.5). */
echo INPUT_POST,INPUT_GET,INPUT_COOKIE,INPUT_ENV,INPUT_SERVER,"\n";

$M = 'DEFINITELY_MISSING_KEY';
/* variable not set: default > false(if NULL_ON_FAILURE) > null */
fi_show(filter_input(INPUT_SERVER, $M));
fi_show(filter_input(INPUT_SERVER, $M, FILTER_DEFAULT, ['options'=>['default'=>'DFT']]));
fi_show(filter_input(INPUT_SERVER, $M, FILTER_DEFAULT, FILTER_NULL_ON_FAILURE));
fi_show(filter_input(INPUT_SERVER, $M, FILTER_VALIDATE_INT, ['flags'=>FILTER_NULL_ON_FAILURE,'options'=>['default'=>'DFT']]));

/* empty superglobals in CLI -> every key is "not set" */
fi_show(filter_input(INPUT_GET, 'x', FILTER_VALIDATE_INT));
fi_show(filter_input(INPUT_POST, 'y'));

/* DOCUMENT_ROOT is '' in both engines' server view: present-but-fails semantics */
fi_show(filter_input(INPUT_SERVER, 'DOCUMENT_ROOT'));
fi_show(filter_input(INPUT_SERVER, 'DOCUMENT_ROOT', FILTER_VALIDATE_EMAIL));
fi_show(filter_input(INPUT_SERVER, 'DOCUMENT_ROOT', FILTER_VALIDATE_EMAIL, ['options'=>['default'=>'DFT']]));
fi_show(filter_input(INPUT_SERVER, 'DOCUMENT_ROOT', FILTER_VALIDATE_EMAIL, FILTER_NULL_ON_FAILURE));

/* SAPI-registered key matches the live superglobal in CLI */
fi_show(filter_input(INPUT_SERVER, 'SCRIPT_NAME') === $_SERVER['SCRIPT_NAME']);
?>
--EXPECT--
01245
NULL
'DFT'
false
'DFT'
NULL
NULL
''
false
'DFT'
NULL
true
