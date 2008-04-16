%{
#include <stdio.h>
#include "parser.tab.h"
%}

%%
"\n"						return CRLF;
[^()<>@,;:\\"/[\]?={} \x00-\x20\x7F]+		return TOKEN;
":"						return COLON;
[()<>@,;\\"/[\]?={} \x00-\x09\x0B-\x0D\x7F]	return SEPARATOR;
%%

