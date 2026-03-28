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
		typedef bool(*PreprocessFunction)(Vec, Iter, void*);

		void setTokens(std::vector<Token>& tokens) {
			this->tokens = &tokens;
		}

		bool preprocess(PreprocessFunction function, void* data) {
			for (auto it = this->tokens->begin(); it != this->tokens->end(); ) {
				if (!function(*this->tokens, it, data)) return false;
			}
			return true;
		}
	};
}

#endif