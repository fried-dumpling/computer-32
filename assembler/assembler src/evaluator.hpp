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
	using namespace lexer_generator;
	using namespace parser_generator;

	template <typename Terminal, typename NonTerminal, typename ASTElement>
	class Evaluator {
	private:
		using u32 = unsigned __int32;
		using u64 = unsigned __int64;
		using Element = convert::Element;

		using AST = typename Parser<Terminal, NonTerminal, ASTElement>::ASTNode;

		AST* tree;
	public:
		typedef bool(*EvaluateFunction)(AST*, void*);
		typedef bool(*TBEvaluateFunction)(std::vector<AST*>&, AST*, void*);

		Evaluator() {
			this->tree = NULL;
		}
		
		void setTree(AST* tree) {
			this->tree = tree;
		}

		bool evaluate(EvaluateFunction function, void* data) {
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
				if (!function(*it, data)) return false;
			}
			return true;
		}

		bool evaluateTB(TBEvaluateFunction function, void* data) {
			std::stack<AST*> stack;
			stack.push(this->tree);
			while (!stack.empty()) {
				AST* cur = stack.top(); stack.pop();
				std::vector<AST*> next;
				if (!function(next, cur, data))
					return false;
				for (auto it = cur->child.begin(); it != cur->child.end(); ++it)
					stack.push(*it);
			}
			return true;
		}
	};
}

#endif