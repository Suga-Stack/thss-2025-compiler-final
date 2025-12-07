lexer grammar SysYLexer;

// 关键字
CONST: 'const';
INT: 'int';
VOID: 'void';
IF: 'if';
ELSE: 'else';
WHILE: 'while';
BREAK: 'break';
CONTINUE: 'continue';
RETURN: 'return';

// 运算符
PLUS: '+';
MINUS: '-';
MUL: '*';
DIV: '/';
MOD: '%';
ASSIGN: '=';
EQ: '==';
NEQ: '!=';
LT: '<';
GT: '>';
LE: '<=';
GE: '>=';
NOT: '!';
AND: '&&';
OR: '||';

// 分隔符
L_PAREN: '(';
R_PAREN: ')';
L_BRACE: '{';
R_BRACE: '}';
L_BRACKET: '[';
R_BRACKET: ']';
COMMA: ',';
SEMICOLON: ';';
QUESTION: '?';
COLON: ':';

// 标识符
IDENT: [a-zA-Z_][a-zA-Z_0-9]*;

// 整数常量
INT_CONST: 
    '0' 
    | [1-9][0-9]* 
    | '0'[0-7]+ 
    | ('0x'|'0X')[0-9a-fA-F]+
    ;

// 空白和注释 - 跳过不生成token
WS: [ \r\n\t]+ -> skip;
LINE_COMMENT: '//' ~[\r\n]* -> skip;
MULTILINE_COMMENT: '/*' .*? '*/' -> skip;