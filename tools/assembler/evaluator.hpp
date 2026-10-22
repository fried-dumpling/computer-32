#ifndef DPICP_EVALUATOR
#define DPICP_EVALUATOR

#include <string>
#include <vector>

#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <bitset>

#include <queue>
#include <stack>

#include <utility>

#include "lexer.hpp"
#include "parser.hpp"

namespace evaluator_generator {
	template <typename Terminal, typename NonTerminal, typename ASTElement>
	class Evaluator {
	private:
		using u32 = unsigned __int32;
		using u64 = unsigned __int64;
		using Element = parser_generator::convert::Element;

		using AST = typename parser_generator::Parser<Terminal, NonTerminal, ASTElement>::ASTNode;

		AST* tree;
	public:
		using Token = typename parser_generator::Parser<Terminal, NonTerminal, ASTElement>::Token;

		typedef int(*EvaluateFunction)(AST*, void*);

		Evaluator() {
			this->tree = NULL;
		}

		void setTree(AST* tree) {
			this->tree = tree;
		}

		int evaluate(Token& errorToken, EvaluateFunction function, void* data) {
			std::vector<AST*> orderVec;
			std::stack<AST*> stack;
			stack.push(this->tree);
			while (!stack.empty()) {
				AST* cur = stack.top(); stack.pop();
				orderVec.push_back(cur);
				for (auto it = cur->child.begin(); it != cur->child.end(); ++it)
					stack.push(*it);
			}
			for (auto it = orderVec.rbegin(); it != orderVec.rend(); ++it) {
				int code = function(*it, data);
				if (code) {
					errorToken = (*it)->srcToken;
					return code;
				}
			}
			errorToken.line = -1;
			return 0;
		}

		//legacy, unused code
		/*
		typedef int(*TBEvaluateFunction)(std::vector<AST*>&, AST*, void*);
		int evaluateTB(TBEvaluateFunction function, void* data) {
			std::stack<AST*> stack;
			stack.push(this->tree);
			while (!stack.empty()) {
				AST* cur = stack.top(); stack.pop();
				std::vector<AST*> next;
				int code = function(next, cur, data);
				if (code) return code;
				for (auto it = cur->child.begin(); it != cur->child.end(); ++it)
					stack.push(*it);
			}
			return 0;
		}*/
	};
}

#endif