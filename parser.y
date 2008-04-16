%{
#include <stdio.h>
#include <string.h>

void yyerror(const char *str)
{
	printf("error: %s\n", str);
}

int yywrap()
{
	return 1;
}

int main()
{
	yyparse();
	return 0;
}
%}

%token CRLF TOKEN SEPARATOR COLON

%%
input: /* empty */
     | input token
;

token: TOKEN CRLF { printf("\tToken\n"); }
;
%%

