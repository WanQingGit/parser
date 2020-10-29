/*
 * qlr.h
 *
 *  Created on: Jun 14, 2019
 *  Author: WanQing<1109162935@qq.com>
 */

#ifndef INCLUDE_QLR_H_
#define INCLUDE_QLR_H_

#include "qgrammar.h"
#include "qtokenizer.h"
#include "typeGram_.h"
//#include "grammar_.h"
#define EXPFLAG MASK(21)
#define NILFLAG MASK(22)
typedef enum {
	C_CHILD = 0, C_NEVER = 1, C_ACCELERABLE = 2, C_ATONCE = 4, C_MERGEABLE = 8
} CallType;
#define  injectFunc(analyzer, funname,fun,boost) {\
		RFunc *_rf=qmalloc(RFunc);\
		_rf->func=fun;\
		RB.insert(analyzer->tableFun, (rbtype)funname, (rbtype)_rf,NULL);\
		_rf->callType=boost;\
}
#define FIRST(an,idx) ({\
	RBTree *_deps = RB.create(NULL,NULL);\
	const RBTree *_first=getFirst(an,_deps, idx);\
	RB.destroy(&_deps, NULL);\
	_first;\
})
typedef struct qexpr {
	lr_gram *gram;
	void *p;
//qvec availTks;
	int tokens[];
} qexpr;
typedef struct lr_item {
	lr_gram *gram;
	int at;
} lr_item;
typedef struct lr_clo_item {
	lr_item *item;
	RBTree *expect;
} lr_clo_item;
typedef struct lr_closure {
	RBTree *clo_items;
//	RBTree *goTable;
	int index;
} lr_closure;
/*
 * tableItems为at为0的item
 * */
typedef struct LRAnalyzer LRAnalyzer;
typedef void (*statHookFn)(struct LRAnalyzer*, void *ctx);
struct LRAnalyzer {
	qgen *gen;
	qvec rules;
	qvec closures;
	qvec funcs;
	RBTree *all_item;
	RBTree *all_expect;
	RBTree *all_closure;
	RBTree *all_cloItem; //all_cloItem会比较expect
	RBTree **tableFirst, **tableItems, **tableDeps, **tableGram, **tableDefExp,
			**tableExp, **tableDefClo;
	RBTree *tableFun;
	byte *tableNul;
	qlogger *logger;
	void *ctx;
	statHookFn statHook;
};
typedef enum {
	RT_NONE, RT_TOKEN, RT_USRDATA, RT_TOKENS
} RetType;
typedef struct {
	qval data;
	RetType retType;
} RetDesc;

typedef enum {
	TD_TOCALL, TD_TK, TD_GRAM, TD_CALLED, TD_KEEP
} typeTD;
typedef struct {
	typeTD flag;
	int nterminal;
	union {
		qvec alltks;
		RetDesc *retdesc;
//		int tk;
		qtk *tk;
	} u;
	lr_gram *gram;
	void *data;
} qgramdesc;
typedef struct RuleInfo {
	qgramdesc *preCalled;
	qgramdesc *desc;
	qbytes *tokens;
	RBTree *exprs;
	qvec exp;
	TypeGram parent;
//	TypeGram typeGram;
	int pos;
} RuleInfo;

typedef INT (*regfun)(void *ctx, RuleInfo *ruleinfo);
typedef struct RFunc {
	regfun func;
	CallType callType;
} RFunc;

bool NUL(LRAnalyzer *analyzer, WordID idx);
void CLOSURE(LRAnalyzer *analyzer, RBTree* clos);
RBTree *countClosure(LRAnalyzer *analyzer, RBTree *deps, TypeWord id,
		RBTree *preExpDef, RBTree *lastExp);

void GO(LRAnalyzer *analyzer, RBTree *goTable, WordID idx,
		lr_clo_item *clo_item);

bool analyse(LRAnalyzer *analyzer, qlexer *lex);
void LR1(LRAnalyzer *analyzer);

extern void printItem(qstrbuf *strbuf, qgen *generator, lr_item *item);
extern void printItems(qgen *gen, RBTree *t);
extern void printExpect(qstrbuf *buf, LRAnalyzer *an, RBTree *t);
extern void printAllExpect(LRAnalyzer *analyzer);
extern void printClosure(LRAnalyzer *analyzer, lr_closure* clo);
void printCloItems(LRAnalyzer *analyzer, RBTree* cloItems);
extern void printClosures(LRAnalyzer *analyzer, qvec clos);
extern void printAllSymbol(qgen *gen);
extern void printStackInfo(qgen *gen, qvec stack, qvec stackInfo);
extern void printfToken(qgen *gen, qbytes *tokens, int top);
extern void printfExpect(qgen *gen, int *rule);

//extern int cmpItem(lr_item* i1, lr_item *i2);
void serialAnalyzer(LRAnalyzer *analyzer, char *path);
LRAnalyzer *deserialAnalyzer(char *path);
LRAnalyzer *createLRAnalyzer(qgen *gen);
void destroyAnalyzer(LRAnalyzer *analyzer);
void gen_file(LRAnalyzer *analyzer, char *path);
#endif /* INCLUDE_QLR_H_ */
