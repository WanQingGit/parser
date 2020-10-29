/*
 * LR1.c
 *
 *  Created on: May 22, 2019
 *  Author: WanQing<1109162935@qq.com>
 */
#include "qvector.h"
#include "qmap.h"
#include "qset.h"
#include "qlist.h"
#include "qmap.h"
#include "rbtree.h"
#include "qstring.h"
#include "qlr.h"
#include "qcontrol.h"
#include "qlogger.h"
#include "qprop.h"
#include "qmem.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

void parserGramFile(qprop *prop, qstr *fileGram);
void loadFromFile(qprop *prop);

int main() {
	struct stat statbuf;
	CTRL.init();
	char temp[20];
	qstr *key_file_gram = STR.get("FILE_GRAM");
	qprop *prop = Prop.read("conf/parser.conf", "=");
	qstr* fileGram = Prop.get(prop, key_file_gram, NULL);
	stat(fileGram->val, &statbuf);
	skyc_assert_(S_ISREG(statbuf.st_mode));
	INT time = statbuf.st_mtim.tv_sec * 1000
			+ statbuf.st_mtim.tv_nsec / (1000 * 1000);
	qstr *s = STR.create(temp, sprintf(temp, "%lld", time));
	qstr *s2 = Prop.get(prop, STR.get("FILE_TEMP"), NULL);
	qprop *fileinfo = Prop.read(s2->val, ":");
	s2 = Prop.get(fileinfo, fileGram, NULL);
	if (s != s2) {
		parserGramFile(prop, fileGram);
		Prop.insert(fileinfo, fileGram, s);
		Prop.write(fileinfo);
	}
	loadFromFile(prop);
	Prop.destroy(fileinfo);
	s = Prop.get(prop, STR.get("DEBUG_MEM"), NULL);
	Prop.destroy(prop);
	if (s && atoi(s->val)) {
		CTRL.destroy(); //destroy会清空字符串，所以要在取出s的值后再调用
		Mem.printMeminfo();
	} else {
		CTRL.destroy();
	}
	return 0;
}
void loadFromFile(qprop *prop) {
	qstr *path = Prop.get(prop, STR.get("DIR_CODE"), STR.get("Code"));
	qstr *fileRules = Prop.get(prop, STR.get("FILE_RULES"), NULL);
	LRAnalyzer *analyzer2 = deserialAnalyzer(fileRules->val);
	qlexer *lex = create_lexer_file("res/Code.txt");
	DIR *dirp;
	struct dirent *entry;
	struct stat statbuf;
	dirp = opendir(path->val); //打开目录指针
	chdir(path->val);
	while ((entry = readdir(dirp)) != NULL) {
		lstat(entry->d_name, &statbuf);
		if (S_ISREG(statbuf.st_mode)) { //S_ISREG判断是否是普通文件
			printf("Start testing file %s ...\n", entry->d_name);
			set_lexer_file(lex, entry->d_name);
			skyc_assert_(analyse(analyzer2, lex));
		}
	}
	chdir("..");
	closedir(dirp); //关闭目录
	lex_destory(lex);
	destroyAnalyzer(analyzer2);
}

void parserGramFile(qprop *prop, qstr *fileGram) {
	qgen *gen = createGen(fileGram->val);
//	qgen *gen = createGen("res/Grammar_BNF");
	initParser(gen);
	parseGram(gen);
	printAllSymbol(gen);
	gen_wordFile(gen, "include/typeWord_.h");
	LRAnalyzer *analyzer = createLRAnalyzer(gen);
	qstr *filelog = Prop.get(prop, STR.get("FILE_LOG"), STR.get("lr.log"));
	qlogger *logger = Log.createByPath(filelog->val);
	logger->policy |= LOG_CONSOLE;
	logger->cachesize = 64;
	analyzer->logger = logger;
	genGramFile(gen, "include/typeGram_.h"); //一定要在createLRAnalyzer之后
	LR1(analyzer);
	qstr *filerules = Prop.get(prop, STR.get("FILE_RULES"),
			STR.get("res/rules.dat"));
	serialAnalyzer(analyzer, filerules->val);
//	qlexer *lex = create_lexer_file("res/Code.txt");
//	analyse(analyzer, lex);
	destroyAnalyzer(analyzer);
}

