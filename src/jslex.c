/*
 * jslex.c
 *
 *  Created on: 1 Nov 2011
 *      Author: gw
 */

#include "jslex.h"


void jslSeek(JsLex *lex, JslCharPos seekToChar) {
  lex->currentPos = seekToChar;
  jsvStringIteratorFree(&lex->it);
  jsvStringIteratorNew(&lex->it, lex->sourceVar, seekToChar);
}

void jslGetNextCh(JsLex *lex) {
  lex->currCh = lex->nextCh;
  if (jsvStringIteratorHasChar(&lex->it)) {
    lex->nextCh = jsvStringIteratorGetChar(&lex->it);
    jsvStringIteratorNext(&lex->it);
  } else
    lex->nextCh = 0;
  lex->currentPos++;
}

static inline void jslTokenAppendChar(JsLex *lex, char ch) {
  /* Add character to buffer but check it isn't too big.
   * Also Leave ONE character at the end for null termination */
  if (lex->tokenl < JSLEX_MAX_TOKEN_LENGTH-1) {
    lex->token[lex->tokenl++] = ch;
  }
#ifdef DEBUG
  else {
    jsWarnAt("Token name is too long! skipping character", lex, lex->tokenStart);
  }
#endif
}

static inline bool jslIsToken(JsLex *lex, const char *token) {
  int i;
  for (i=0;i<lex->tokenl;i++) {
    if (lex->token[i]!=token[i]) return false;
    // if token is smaller than lex->token, there will be a null char
    // which will be different from the token
  }
  return token[lex->tokenl] == 0; // only match if token ends now
}

void jslGetNextToken(JsLex *lex) {
  lex->tk = LEX_EOF;
  lex->tokenl = 0; // clear token string
  if (lex->tokenValue) {
    jsvUnLock(lex->tokenValue);
    lex->tokenValue = 0;
  }
  while (lex->currCh && isWhitespace(lex->currCh)) jslGetNextCh(lex);
  // newline comments
  if (lex->currCh=='/' && lex->nextCh=='/') {
      while (lex->currCh && lex->currCh!='\n') jslGetNextCh(lex);
      jslGetNextCh(lex);
      jslGetNextToken(lex);
      return;
  }
  // block comments
  if (lex->currCh=='/' && lex->nextCh=='*') {
      while (lex->currCh && (lex->currCh!='*' || lex->nextCh!='/')) jslGetNextCh(lex);
      jslGetNextCh(lex);
      jslGetNextCh(lex);
      jslGetNextToken(lex);
      return;
  }
  // record beginning of this token
  lex->tokenLastStart = lex->tokenStart;
  lex->tokenStart = (JslCharPos)(lex->currentPos-2);
  // tokens
  if (isAlpha(lex->currCh)) { //  IDs
      while (isAlpha(lex->currCh) || isNumeric(lex->currCh)) {
          jslTokenAppendChar(lex, lex->currCh);
          jslGetNextCh(lex);
      }
      lex->tk = LEX_ID;
           //OPT: could do fancy nested IFs here to reduce number of compares
           if (jslIsToken(lex,"if")) lex->tk = LEX_R_IF;
      else if (jslIsToken(lex,"else")) lex->tk = LEX_R_ELSE;
      else if (jslIsToken(lex,"do")) lex->tk = LEX_R_DO;
      else if (jslIsToken(lex,"while")) lex->tk = LEX_R_WHILE;
      else if (jslIsToken(lex,"for")) lex->tk = LEX_R_FOR;
      else if (jslIsToken(lex,"break")) lex->tk = LEX_R_BREAK;
      else if (jslIsToken(lex,"continue")) lex->tk = LEX_R_CONTINUE;
      else if (jslIsToken(lex,"function")) lex->tk = LEX_R_FUNCTION;
      else if (jslIsToken(lex,"return")) lex->tk = LEX_R_RETURN;
      else if (jslIsToken(lex,"var")) lex->tk = LEX_R_VAR;
      else if (jslIsToken(lex,"true")) lex->tk = LEX_R_TRUE;
      else if (jslIsToken(lex,"false")) lex->tk = LEX_R_FALSE;
      else if (jslIsToken(lex,"null")) lex->tk = LEX_R_NULL;
      else if (jslIsToken(lex,"undefined")) lex->tk = LEX_R_UNDEFINED;
      else if (jslIsToken(lex,"new")) lex->tk = LEX_R_NEW;
      else if (jslIsToken(lex,"in")) lex->tk = LEX_R_IN;
      else if (jslIsToken(lex,"switch")) lex->tk = LEX_R_SWITCH;
      else if (jslIsToken(lex,"case")) lex->tk = LEX_R_CASE;
      else if (jslIsToken(lex,"default")) lex->tk = LEX_R_DEFAULT;
  } else if (isNumeric(lex->currCh)) { // Numbers
      // TODO: check numbers aren't the wrong format
      bool canBeFloating = true;
      if (lex->currCh=='0') { 
        jslTokenAppendChar(lex, lex->currCh); 
        jslGetNextCh(lex); 
      }
      if (lex->currCh=='x' || lex->currCh=='b') {
        canBeFloating = false;
        jslTokenAppendChar(lex, lex->currCh); jslGetNextCh(lex);
      }
      lex->tk = LEX_INT;
      while (isNumeric(lex->currCh) || (!canBeFloating && isHexadecimal(lex->currCh))) {
        jslTokenAppendChar(lex, lex->currCh);
        jslGetNextCh(lex);
      }
      if (canBeFloating && lex->currCh=='.') {
          lex->tk = LEX_FLOAT;
          jslTokenAppendChar(lex, '.');
          jslGetNextCh(lex);
          while (isNumeric(lex->currCh)) {
            jslTokenAppendChar(lex, lex->currCh);
            jslGetNextCh(lex);
          }
      }
      // do fancy e-style floating point
      if (canBeFloating && (lex->currCh=='e'||lex->currCh=='E')) {
        lex->tk = LEX_FLOAT;
        jslTokenAppendChar(lex, lex->currCh); jslGetNextCh(lex);
        if (lex->currCh=='-') { jslTokenAppendChar(lex, lex->currCh); jslGetNextCh(lex); }
        while (isNumeric(lex->currCh)) {
          jslTokenAppendChar(lex, lex->currCh); jslGetNextCh(lex);
        }
      }
  } else if (lex->currCh=='"' || lex->currCh=='\'') {
      char delim = lex->currCh;
      lex->tokenValue = jsvNewFromString("");
      // strings...
      jslGetNextCh(lex);
      while (lex->currCh && lex->currCh!=delim) {
          if (lex->currCh == '\\') {
              jslGetNextCh(lex);
              char ch = lex->currCh;
              switch (lex->currCh) {
              case 'n'  : ch = '\n'; break;
              case 'a'  : ch = '\a'; break;
              case 'r'  : ch = '\r'; break;
              case 't'  : ch = '\t'; break;
              case '\'' : ch = '\''; break;
              case '\\' : ch = '\\'; break;
              case 'x' : { // hex digits
                            char buf[5] = "0x??";
                            jslGetNextCh(lex); buf[2] = lex->currCh;
                            jslGetNextCh(lex); buf[3] = lex->currCh;
                            ch = (char)stringToInt(buf);
                         } break;
              default: 
                       if (lex->currCh>='0' && lex->currCh<='7') {
                         // octal digits
                         char buf[5] = "0";
                         buf[1] = lex->currCh;
                         int n=2;
                         if (lex->currCh>='0' && lex->currCh<='7') {
                           jslGetNextCh(lex); buf[n++] = lex->currCh;
                         }
                         if (lex->currCh>='0' && lex->currCh<='7') {
                           jslGetNextCh(lex); buf[n++] = lex->currCh;
                         }
                         buf[n]=0;
                         ch = (char)stringToInt(buf);
                       }
                       break;
              }
              if (lex->tokenValue) {
                jslTokenAppendChar(lex, ch);
                jsvAppendCharacter(lex->tokenValue, ch);
              }
          } else {
            if (lex->tokenValue) {
              jslTokenAppendChar(lex, lex->currCh);
              jsvAppendCharacter(lex->tokenValue, lex->currCh);
            }
          }
          jslGetNextCh(lex);
      }
      jslGetNextCh(lex);
      lex->tk = LEX_STR;
  } else {
      // single chars
      lex->tk = lex->currCh;
      if (lex->currCh) jslGetNextCh(lex);
      if (lex->tk=='=' && lex->currCh=='=') { // ==
          lex->tk = LEX_EQUAL;
          jslGetNextCh(lex);
          if (lex->currCh=='=') { // ===
            lex->tk = LEX_TYPEEQUAL;
            jslGetNextCh(lex);
          }
      } else if (lex->tk=='!' && lex->currCh=='=') { // !=
          lex->tk = LEX_NEQUAL;
          jslGetNextCh(lex);
          if (lex->currCh=='=') { // !==
            lex->tk = LEX_NTYPEEQUAL;
            jslGetNextCh(lex);
          }
      } else if (lex->tk=='<' && lex->currCh=='=') {
          lex->tk = LEX_LEQUAL;
          jslGetNextCh(lex);
      } else if (lex->tk=='<' && lex->currCh=='<') {
          lex->tk = LEX_LSHIFT;
          jslGetNextCh(lex);
          if (lex->currCh=='=') { // <<=
            lex->tk = LEX_LSHIFTEQUAL;
            jslGetNextCh(lex);
          }
      } else if (lex->tk=='>' && lex->currCh=='=') {
          lex->tk = LEX_GEQUAL;
          jslGetNextCh(lex);
      } else if (lex->tk=='>' && lex->currCh=='>') {
          lex->tk = LEX_RSHIFT;
          jslGetNextCh(lex);
          if (lex->currCh=='=') { // >>=
            lex->tk = LEX_RSHIFTEQUAL;
            jslGetNextCh(lex);
          } else if (lex->currCh=='>') { // >>>
            lex->tk = LEX_RSHIFTUNSIGNED;
            jslGetNextCh(lex);
          }
      }  else if (lex->tk=='+' && lex->currCh=='=') {
          lex->tk = LEX_PLUSEQUAL;
          jslGetNextCh(lex);
      }  else if (lex->tk=='-' && lex->currCh=='=') {
          lex->tk = LEX_MINUSEQUAL;
          jslGetNextCh(lex);
      }  else if (lex->tk=='+' && lex->currCh=='+') {
          lex->tk = LEX_PLUSPLUS;
          jslGetNextCh(lex);
      }  else if (lex->tk=='-' && lex->currCh=='-') {
          lex->tk = LEX_MINUSMINUS;
          jslGetNextCh(lex);
      } else if (lex->tk=='&' && lex->currCh=='=') {
          lex->tk = LEX_ANDEQUAL;
          jslGetNextCh(lex);
      } else if (lex->tk=='&' && lex->currCh=='&') {
          lex->tk = LEX_ANDAND;
          jslGetNextCh(lex);
      } else if (lex->tk=='|' && lex->currCh=='=') {
          lex->tk = LEX_OREQUAL;
          jslGetNextCh(lex);
      } else if (lex->tk=='|' && lex->currCh=='|') {
          lex->tk = LEX_OROR;
          jslGetNextCh(lex);
      } else if (lex->tk=='^' && lex->currCh=='=') {
          lex->tk = LEX_XOREQUAL;
          jslGetNextCh(lex);
      }
  }
  /* This isn't quite right yet */
  lex->tokenLastEnd = lex->tokenEnd;
  lex->tokenEnd = (JslCharPos)(lex->currentPos-3)/*because of nextCh/currCh/etc */;
}

static inline void jslPreload(JsLex *lex) {
  // set up..
  jslGetNextCh(lex);
  jslGetNextCh(lex);
  jslGetNextToken(lex);
}

void jslInit(JsLex *lex, JsVar *var) {
  lex->sourceVar = jsvLockAgain(var);
  // reset stuff
  lex->currentPos = 0;
  lex->tk = 0;
  lex->tokenStart = 0;
  lex->tokenEnd = 0;
  lex->tokenLastStart = 0;
  lex->tokenLastEnd = 0;
  lex->tokenl = 0;
  lex->tokenValue = 0;
  // set up iterator
  jsvStringIteratorNew(&lex->it, lex->sourceVar, 0);
  jslPreload(lex);
}

void jslKill(JsLex *lex) {
  lex->tk = LEX_EOF; // safety ;)
  jsvStringIteratorFree(&lex->it);
  if (lex->tokenValue) {
    jsvUnLock(lex->tokenValue);
    lex->tokenValue = 0;
  }
  jsvUnLock(lex->sourceVar);
}

void jslSeekTo(JsLex *lex, JslCharPos seekToChar) {
  jslSeek(lex, seekToChar);
  jslPreload(lex);
}

void jslReset(JsLex *lex) {
  jslSeekTo(lex, 0);
}

void jslTokenAsString(int token, char *str, size_t len) {
  // see JS_ERROR_TOKEN_BUF_SIZE
  if (token>32 && token<128) {
      assert(len>=4);
      str[0] = '\'';
      str[1] = (char)token;
      str[2] = '\'';
      str[3] = 0;
      return;
  }
  switch (token) {
      case LEX_EOF : strncpy(str, "EOF", len); return;
      case LEX_ID : strncpy(str, "ID", len); return;
      case LEX_INT : strncpy(str, "INT", len); return;
      case LEX_FLOAT : strncpy(str, "FLOAT", len); return;
      case LEX_STR : strncpy(str, "STRING", len); return;
      case LEX_EQUAL : strncpy(str, "==", len); return;
      case LEX_TYPEEQUAL : strncpy(str, "===", len); return;
      case LEX_NEQUAL : strncpy(str, "!=", len); return;
      case LEX_NTYPEEQUAL : strncpy(str, "!==", len); return;
      case LEX_LEQUAL : strncpy(str, "<=", len); return;
      case LEX_LSHIFT : strncpy(str, "<<", len); return;
      case LEX_LSHIFTEQUAL : strncpy(str, "<<=", len); return;
      case LEX_GEQUAL : strncpy(str, ">=", len); return;
      case LEX_RSHIFT : strncpy(str, ">>", len); return;
      case LEX_RSHIFTUNSIGNED : strncpy(str, ">>", len); return;
      case LEX_RSHIFTEQUAL : strncpy(str, ">>=", len); return;
      case LEX_PLUSEQUAL : strncpy(str, "+=", len); return;
      case LEX_MINUSEQUAL : strncpy(str, "-=", len); return;
      case LEX_PLUSPLUS : strncpy(str, "++", len); return;
      case LEX_MINUSMINUS : strncpy(str, "--", len); return;
      case LEX_ANDEQUAL : strncpy(str, "&=", len); return;
      case LEX_ANDAND : strncpy(str, "&&", len); return;
      case LEX_OREQUAL : strncpy(str, "|=", len); return;
      case LEX_OROR : strncpy(str, "||", len); return;
      case LEX_XOREQUAL : strncpy(str, "^=", len); return;
              // reserved words
      case LEX_R_IF : strncpy(str, "if", len); return;
      case LEX_R_ELSE : strncpy(str, "else", len); return;
      case LEX_R_DO : strncpy(str, "do", len); return;
      case LEX_R_WHILE : strncpy(str, "while", len); return;
      case LEX_R_FOR : strncpy(str, "for", len); return;
      case LEX_R_BREAK : strncpy(str, "return", len); return;
      case LEX_R_CONTINUE : strncpy(str, "continue", len); return;
      case LEX_R_FUNCTION : strncpy(str, "function", len); return;
      case LEX_R_RETURN : strncpy(str, "return", len); return;
      case LEX_R_VAR : strncpy(str, "var", len); return;
      case LEX_R_TRUE : strncpy(str, "true", len); return;
      case LEX_R_FALSE : strncpy(str, "false", len); return;
      case LEX_R_NULL : strncpy(str, "null", len); return;
      case LEX_R_UNDEFINED : strncpy(str, "undefined", len); return;
      case LEX_R_NEW : strncpy(str, "new", len); return;
      case LEX_R_IN : strncpy(str, "in", len); return;
  }
  assert(len>=10);
  strncpy(str, "?[",len);
  itoa(token, &str[2], 10);
  strncat(str, "]",len);
}

void jslGetTokenString(JsLex *lex, char *str, size_t len) {
  if (lex->tk == LEX_ID) {
    strncpy(str, "ID:", len);
    strncat(str, jslGetTokenValueAsString(lex), len);
  } else if (lex->tk == LEX_STR) {
    strncpy(str, "String:'", len);
    strncat(str, jslGetTokenValueAsString(lex), len);
    strncat(str, "'", len);
  } else
    jslTokenAsString(lex->tk, str, len);
}

char *jslGetTokenValueAsString(JsLex *lex) {
  assert(lex->tokenl < JSLEX_MAX_TOKEN_LENGTH);
  lex->token[lex->tokenl]  = 0; // add final null
  return lex->token;
}

JsVar *jslGetTokenValueAsVar(JsLex *lex) {
  if (lex->tokenValue) {
    return jsvLockAgain(lex->tokenValue);
  } else {
    assert(lex->tokenl < JSLEX_MAX_TOKEN_LENGTH);
    lex->token[lex->tokenl]  = 0; // add final null
    return jsvNewFromString(lex->token);
  }
}

/// Match, and return true on success, false on failure
bool jslMatch(JsLex *lex, int expected_tk) {
  if (lex->tk!=expected_tk) {
      char buf[JS_ERROR_BUF_SIZE];
      size_t bufpos = 0;
      strncpy(&buf[bufpos], "Got ", JS_ERROR_BUF_SIZE-bufpos);
      bufpos = strlen(buf);
      jslGetTokenString(lex, &buf[bufpos], JS_ERROR_BUF_SIZE-bufpos);
      bufpos = strlen(buf);
      strncpy(&buf[bufpos], " expected ", JS_ERROR_BUF_SIZE-bufpos);
      bufpos = strlen(buf);
      jslTokenAsString(expected_tk, &buf[bufpos], JS_ERROR_BUF_SIZE-bufpos);
      jsErrorAt(buf, lex, lex->tokenStart);
      // Sod it, skip this token anyway - stops us looping
      jslGetNextToken(lex);
      return false;
  }
  jslGetNextToken(lex);
  return true;
}

