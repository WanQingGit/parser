/*
 * qgrammar.h
 *
 *  Created on: Jun 20, 2019
 *  Author: WanQing<1109162935@qq.com>
 */

#ifndef INCLUDE_QGRAMMAR_H_
#define INCLUDE_QGRAMMAR_H_
#include "rbtree.h"
#include "qtoken_gen.h"
#include "qstring.h"
#include "typeGram_.h"
#define NT_FORMAT  "%s.%d"
#define  destroyTypeGram() \
	if (typeGram != NULL){\
		destroyType(typeGram->name);\
		typeGram = NULL;\
}
#define isGram(gen,gram) usrGram(gen,gram->left)
void createTypeGram();
#define usrGram(gen,i) (strchr(gsymbol(gen,i),'.')==NULL)
typedef struct lr_gram {
	qvec right;
	TypeGram nval;
	TypeWord left;
} lr_gram;
void printGram(qstrbuf *buf,qgen *generator, lr_gram *grammar);
void parseGram(qgen *gen);
int lr_or(qgen *gen, TypeWord name, WordID left);
int lr_group(qgen *gen, TypeWord name);
int lr_optional(qgen *gen, TypeWord name);
int lr_multi(qgen *gen, TypeWord name);
extern int cmpGram(lr_gram *g1, lr_gram *g2);
void initParser(qgen *gen);
void genGramFile(qgen *gen, char *path);
//extern RBTree all_gram;
//extern qvector  grams;
extern Type typeGram;
#endif /* INCLUDE_QGRAMMAR_H_ */
