parser grammar SysYParser;

options {
      tokenVocab = SysYLexer;
}

compUnit: (decl | funcDef)* EOF;

decl: constDecl | varDecl;

constDecl: CONST bType constDef (COMMA constDef)* SEMICOLON;

bType: INT;

// allow missing size in first bracket (e.g. IDENT[][3]) by making constExp optional
constDef: IDENT (L_BRACKET constExp R_BRACKET)* ASSIGN constInitVal;

constInitVal: constExp
            | L_BRACE (constInitVal (COMMA constInitVal)*)? R_BRACE;

varDecl: bType varDef (COMMA varDef)* SEMICOLON;

// varDef: identifier with zero or more bracket dimensions (each size must be a constExp),
// optionally followed by an initializer
varDef: IDENT (L_BRACKET constExp R_BRACKET)* (ASSIGN initVal)?;

initVal: exp
       | L_BRACE (initVal (COMMA initVal)*)? R_BRACE;

funcDef: funcType IDENT L_PAREN (funcFParams)? R_PAREN block;

funcType: VOID | INT;

funcFParams: funcFParam (COMMA funcFParam)*;

// allow pointer parameter like: int *c
// function parameter may be pointer or array; first [] may be empty or sized,
// followed by zero or more sized brackets: e.g., int a[][3][4] or int a[10][3]
funcFParam: bType (MUL)? IDENT (L_BRACKET (constExp)? R_BRACKET (L_BRACKET constExp R_BRACKET)*)?;

block: L_BRACE blockItem* R_BRACE;

blockItem: decl | stmt;

stmt: lVal ASSIGN exp SEMICOLON           # assignStmt
    | (exp)? SEMICOLON                    # expStmt
    | block                               # blockStmt
    | IF L_PAREN cond R_PAREN stmt (ELSE stmt)? # ifStmt
    | WHILE L_PAREN cond R_PAREN stmt     # whileStmt
    | BREAK SEMICOLON                     # breakStmt
    | CONTINUE SEMICOLON                  # continueStmt
    | RETURN (exp)? SEMICOLON             # returnStmt
    ;

exp: addExp;

// cond supports ternary operator: a ? b : c (right-associative)
cond: lOrExp (QUESTION exp COLON cond)?;

lVal: IDENT (L_BRACKET exp R_BRACKET)*;

primaryExp: L_PAREN exp R_PAREN
          | lVal
          | number
          ;

number: INT_CONST;

unaryExp: primaryExp
        | IDENT L_PAREN (funcRParams)? R_PAREN
        | unaryOp unaryExp
        ;

unaryOp: PLUS | MINUS | NOT;

funcRParams: exp (COMMA exp)*;

mulExp: unaryExp
      | mulExp (MUL | DIV | MOD) unaryExp
      ;

addExp: mulExp
      | addExp (PLUS | MINUS) mulExp
      ;

relExp: addExp
      | relExp (LT | GT | LE | GE) addExp
      ;

eqExp: relExp
     | eqExp (EQ | NEQ) relExp
     ;

lAndExp: eqExp
       | lAndExp AND eqExp
       ;

lOrExp: lAndExp
      | lOrExp OR lAndExp
      ;

constExp: addExp;
