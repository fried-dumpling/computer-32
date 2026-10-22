#ifndef DPICP_PREPROCESSER
#define DPICP_PREPROCESSER

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

namespace preproccesser_generator {
	using namespace lexer_generator;

	template <typename Token>
	class Preprocesser {
	private:
		using u32 = unsigned __int32;
		using u64 = unsigned __int64;

		std::vector<Token>* tokens;
	public:
		using Vec = std::vector<Token>&;
		using Iter = typename std::vector<Token>::iterator&;
		typedef int(*PreprocessFunction)(Vec, Iter, void*);

		void setTokens(std::vector<Token>& tokens) {
			this->tokens = &tokens;
		}

		typename std::vector<Token>::iterator begin() {
			return this->tokens->begin();
		}

		// on failure errorToken is the token the callback was looking at (line = -1 past the last token)
		int preprocess(Token& errorToken, PreprocessFunction function, void* data) {
			auto it = this->tokens->begin();
			for (; it != this->tokens->end(); ) {
				int code = function(*this->tokens, it, data);
				if (code) {
					if (it != this->tokens->end())
						errorToken = *it;
					else {
						errorToken = {};
						errorToken.line = -1;
					}
					return code;
				}
			}
			return 0;
		}
	};
}

#endif