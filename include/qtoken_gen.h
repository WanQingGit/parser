/*
 * qtoken_gen.h
 *
 *  Created on: May 24, 2019
 *  Author: WanQing<1109162935@qq.com>
 */

#ifndef INCLUDE_QTOKEN_GEN_H_
#define INCLUDE_QTOKEN_GEN_H_
#include "qstrutils.h"
#include "qvector.h"
#include "rbtree.h"
#include "qstring.h"
#include "typeWord_.h"
#define BUFSIZE 64
#define gtype(gen) ((gen)->tk.type)
#define ginfo(gen) ((gen)->tk.info)
#define gsymbol(gen,i) gsymStr(gen,i)->val
#define gsymStr(gen,i) (gen)->symbols->data[i].s
#define NTFLAG MASK(19)

#define RFLAG MASK(21)
#define RMASK MASKN(21,0)
#define IDMASK MASKN(19,0)

#define create_terminal(gen,name) add_terminal(gen,STR.create( name, strlen(name)))
#define create_nt(gen,name) add_nt(gen,STR.create( name, strlen(name)))
// @formatter:off
//enum {
//	TW_START,TW_STAT,TW_NAME, TW_NUM,TW_BOOL, TW_STRING, /*ID_FLT, ID_INT,*/
//	 TW_END,TW_NULL,TW_NLINE, TW_NIL,
//};
//enum{
//	WI_END=WI_END|TFLAG,WI_NLINE=TW_NLINE|TFLAG,WI_NIL=TW_NIL|TFLAG
//};
extern const char *buildIn[];

//@formatter:on
typedef enum {
	TAB_EOS,
	TAB_NT,
	TAB_NEWNT,
	TAB_INT,
	TAB_TERMINAL,
	TAB_GROUP = '(',
	TAB_GREND = ')',
	TAB_OR = '|',
	TAB_OPTIONAL = '[',
	TAB_OPTEND = ']',
	TAB_MULTI = '{',
	TAB_MULTIEND = '}'
} tktype;
typedef struct tkdesc {
	tktype type;
	WordID info;
} tkdesc;

typedef struct qgen {
	qvec symbols;
	qvec grams;
	RBTree *terminals; //id(int) in symbols-str(qstr)
	RBTree *nterminals;
	RBTree *all_gram;//grammar-gramIdx in grams
	RBTree **tableGram;
	RBTree *numGen;
	char *content;
	char *ptr;
	tkdesc tk;
	byte nbuf;
	char buf[BUFSIZE];
} qgen;

qgen* createGen(char *path);
void destroyGen(qgen* gen);
tktype next_word(qgen *gen);
WordID add_nt(qgen *gen, qstr *str);
WordID add_terminal(qgen *gen, qstr *str);
void gen_wordFile(qgen *gen, char *path);
//extern qgen *generator;

#endif /* INCLUDE_QTOKEN_GEN_H_ */
