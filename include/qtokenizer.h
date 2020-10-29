/*
 * qtokenizer.h
 *
 *  Created on: May 29, 2019
 *  Author: WanQing<1109162935@qq.com>
 */
#ifndef INCLUDE_QTOKENIZER_H_
#define INCLUDE_QTOKENIZER_H_
#include "qobject.h"
#include "typeWord_.h"
//#include "qlist.h"
#include "bytelist.h"
#define LEX_BUFSIZE 4096
#define ID_MAX_LEN 64

typedef struct qtk {
	TypeWord type;
	qval v;
} qtk;
typedef void (*tkHookfn)(void *ctx, qtk *);
typedef struct qlexer {
	char read[LEX_BUFSIZE];
	char buffer[ID_MAX_LEN];
	int nbuf;
	char* ptr;
	char *tail; //指向最后一个字符
	void (*fill)(struct qlexer*);
	int (*close)(void *);
	void *file;
	tkHookfn tokenHook;
	qbytes *tkcache;
//	int cachesize;
	int ncache;
	union {
		INT i;
		qobj *o;
		qstr *s;
	} u;
	uint curline;
	void *ctx;
} qlexer;
qlexer* create_lexer_file(char *path);
int gen_tokens(qlexer *lex, int num);
void set_lexer_file(qlexer *lex, char *path);
void lex_destory(qlexer *lex);
#endif /* INCLUDE_QTOKENIZER_H_ */
