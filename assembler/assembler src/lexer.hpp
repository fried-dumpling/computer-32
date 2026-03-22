#ifndef DPICP_LEXER
#define DPICP_LEXER

#include <iostream>

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

#define TOKENSTART(name) enum class name { __unknown = -1, __epsilon = 0,
#define TOKENEND() __eot, __end }

namespace lexer_generator {
	namespace graph {
		using ID = unsigned long long;
		class UIDSet {
		private:
			std::set<ID> used;
			std::set<ID> free;
		public:
			ID genID(void) {
				ID id = 0;
				if (this->free.empty()) {
					if (!this->used.empty())
						id = *this->used.rbegin() + 1;
				}
				else
					id = *this->free.begin();
				this->free.erase(id);
				this->used.insert(id);
				return id;
			}

			ID genID(ID id) {
				this->free.erase(id);
				this->used.insert(id);
				return id;
			}

			void delID(ID id) {
				this->free.insert(id);
				this->used.erase(id);

				auto it = this->free.rbegin();
				ID top = *this->used.rbegin();
				while (it != this->free.rend() && *it > top) {
					this->free.erase(*it);
					it = this->free.rbegin();
				}
			}

			void clear() {
				this->free.clear();
				this->used.clear();
			}

			void getAll(std::vector<ID>& list) const {
				for (auto it = this->used.begin(); it != this->used.end(); ++it)
					list.push_back(*it);
			}

			void getAll(std::unordered_set<ID>& list) const {
				for (auto it = this->used.begin(); it != this->used.end(); ++it)
					list.insert(*it);
			}

			size_t getSize() const {
				return this->used.size();
			}

			ID getMax() const {
				if (this->used.empty())
					return 0;
				return *this->used.rbegin();
			}
		};

		void createMap(std::unordered_map<ID, ID>& map, UIDSet& base, const UIDSet& add) {
			map.clear();

			std::vector<ID> list;
			add.getAll(list);
			for (auto it = list.begin(); it != list.end(); ++it) {
				map.insert({ *it, base.genID() });
			}
		}

		typedef struct _Transition {
			ID from;
			ID to;
			std::bitset<128> symbols;
			bool epsilon;

			bool compare(std::bitset<128>& input) {
				if (this->epsilon)
					return true;
				return (this->symbols & input).any();
			}
		} Transition;

		typedef struct _Graph {
			UIDSet set;
			ID start;
			std::unordered_set<ID> end;
			std::vector<Transition> transitions;
		} Graph;

		void print(Graph& NFA) {
			std::cout << "start: " << NFA.start << std::endl;
			int count = 0;
			for (auto it = NFA.end.begin(); it != NFA.end.end(); ++it)
				std::cout << "end" << count++ << ": " << *it << std::endl;
			count = 0;
			for (auto it = NFA.transitions.begin(); it != NFA.transitions.end(); ++it) {
				std::cout << "transition" << count++ << " | from: " << it->from << ", to: " << it->to << ", symbol: ";
				if (it->epsilon)
					std::cout << "epsilon" << ", ";
				for (int c = 0; c < 128; c++) {
					if (it->symbols.test(c))
						std::cout << (char)c << ", ";
				}
				std::cout << std::endl;
			}
		}

		namespace generate {
			Graph createEpsilon() {
				Graph graph = {};
				graph.start = graph.set.genID();
				graph.end.insert(graph.set.genID());

				Transition transition = {};
				transition.from = graph.start;
				transition.to = *graph.end.begin();
				transition.symbols = 0;
				transition.epsilon = true;

				graph.transitions.push_back(transition);

				return graph;
			}

			Graph create(char c) {
				Graph graph = {};
				graph.start = graph.set.genID();
				graph.end.insert(graph.set.genID());

				Transition transition = {};
				transition.from = graph.start;
				transition.to = *graph.end.begin();
				transition.symbols.set((unsigned char)c);
				transition.epsilon = false;

				graph.transitions.push_back(transition);

				return graph;
			}

			Graph createClass(std::bitset<128> symbols) {
				Graph graph = {};
				graph.start = graph.set.genID();
				graph.end.insert(graph.set.genID());

				Transition transition = {};
				transition.from = graph.start;
				transition.to = *graph.end.begin();
				transition.symbols = symbols;
				transition.epsilon = false;

				graph.transitions.push_back(transition);

				return graph;
			}

			void clear(Graph& graph) {
				graph.start = 0;
				graph.end.clear();
				graph.set.clear();
				graph.transitions.clear();
			}

			Graph chain(const Graph& from, const Graph& to) {
				using MAP = std::unordered_map<ID, ID>;

				Graph graph;
				MAP idMap1, idMap2;
				createMap(idMap1, graph.set, from.set);

				for (auto it = from.transitions.begin(); it != from.transitions.end(); ++it)
					graph.transitions.push_back({ idMap1[it->from], idMap1[it->to], it->symbols, it->epsilon });
				graph.start = idMap1[from.start];

				for (auto it = from.end.begin(); it != from.end.end(); ++it) {
					idMap2.clear();
					createMap(idMap2, graph.set, to.set);

					for (auto si = to.transitions.begin(); si != to.transitions.end(); ++si)
						graph.transitions.push_back({ idMap2[si->from], idMap2[si->to], si->symbols, si->epsilon });

					for (auto si = to.end.begin(); si != to.end.end(); ++si)
						graph.end.insert(idMap2[*si]);
					graph.transitions.push_back({ idMap1[*it], idMap2[to.start], 0, true });
				}

				return graph;
			}

			Graph junction(const Graph& A, const Graph& B) {
				using MAP = std::unordered_map<ID, ID>;

				Graph graph;
				graph.start = graph.set.genID();
				graph.end.insert(graph.set.genID());
				MAP idMap1, idMap2;
				createMap(idMap1, graph.set, A.set);
				createMap(idMap2, graph.set, B.set);

				for (auto it = A.transitions.begin(); it != A.transitions.end(); ++it)
					graph.transitions.push_back({ idMap1[it->from], idMap1[it->to], it->symbols, it->epsilon });

				for (auto it = B.transitions.begin(); it != B.transitions.end(); ++it)
					graph.transitions.push_back({ idMap2[it->from], idMap2[it->to], it->symbols, it->epsilon });

				graph.transitions.push_back({ graph.start, idMap1[A.start], 0, true });
				graph.transitions.push_back({ graph.start, idMap2[B.start], 0, true });

				for (auto it = A.end.begin(); it != A.end.end(); ++it)
					graph.transitions.push_back({ idMap1[*it], *graph.end.begin(), 0, true });
				for (auto it = B.end.begin(); it != B.end.end(); ++it)
					graph.transitions.push_back({ idMap2[*it], *graph.end.begin(), 0, true });

				return graph;
			}

			Graph split(const std::vector<Graph>& graphs, std::unordered_map<ID, std::pair<ID, ID>>& endMap) {
				using MAP = std::unordered_map<ID, ID>;

				Graph graph;
				graph.start = graph.set.genID();
				std::vector<MAP> idMaps;
				for (auto it = graphs.begin(); it != graphs.end(); ++it) {
					idMaps.push_back(MAP());
					createMap(idMaps.back(), graph.set, it->set);
				}

				size_t index = 0;
				for (auto it = graphs.begin(); it != graphs.end(); ++it) {
					for (auto si = it->transitions.begin(); si != it->transitions.end(); ++si)
						graph.transitions.push_back({ idMaps[index][si->from], idMaps[index][si->to], si->symbols, si->epsilon });
					index++;
				}

				for (index = 0; index < graphs.size(); index++)
					graph.transitions.push_back({ graph.start, idMaps[index][graphs[index].start], 0, true });


				index = 0;
				for (auto it = graphs.begin(); it != graphs.end(); ++it) {
					for (auto si = it->end.begin(); si != it->end.end(); ++si) {
						graph.end.insert(idMaps[index][*si]);
						endMap[idMaps[index][*si]] = { index, *si };
					}
					index++;
				}

				return graph;
			}
		}
	}

	namespace regex {
		enum class TokenType {
			CLASS_OPEN,
			INVCLASS_OPEN,
			CLASS_CLOSE,
			CLASS_SEPERATOR,

			RANGE,

			LOOP,

			QUESTION,
			PLUS,
			STAR,

			OR,
			CONCAT,

			OPENPAREN,
			CLOSEPAREN,

			DOT,

			CHAR,

			UNKNOWN
		};

		typedef struct _Token {
			std::string text;
			TokenType type;
			int data1;
			int data2;
		} Token;

		namespace lexer {
			enum class C {
				NORMAL,
				EXCHAR,
				CLASS,
				LOOP
			};

			enum class S {
				N_START,

				E_START,

				C_START,
				C_MID,

				L_START,
				L_FIRST,
				L_VOID,
				L_SECOND,
				L_END,
			};

			typedef struct _Context {
				C context;
				S state;
			} Context;

			bool isConBegin(TokenType type) {
				switch (type) {
				case TokenType::CLASS_CLOSE:
				case TokenType::LOOP:
				case TokenType::QUESTION:
				case TokenType::PLUS:
				case TokenType::STAR:
				case TokenType::CLOSEPAREN:
				case TokenType::DOT:
				case TokenType::CHAR: return true;
				default: return false;
				}
			}

			void insertConcat(std::vector<Token>& tokens) {
				if (tokens.empty())
					return;
				if (isConBegin(tokens.back().type))
					tokens.push_back({ ".", TokenType::CONCAT, 0, 0 });
			}

			void insertSeperator(std::vector<Token>& tokens) {
				if (tokens.empty())
					return;
				if (tokens.back().type == TokenType::CHAR)
					tokens.push_back({ "|", TokenType::CLASS_SEPERATOR, 0, 0 });
			}

			bool NormalProc(std::vector<Context>& contexts, char c, std::vector<Token>& tokens) {
				auto cur = contexts.rbegin();
				switch (cur->state) {
				case S::N_START:
					switch (c) {
					case '[': insertConcat(tokens); tokens.push_back({ "[", TokenType::CLASS_OPEN, 0, 0 }); contexts.push_back({ C::CLASS, S::C_START }); return true;
					case '{': tokens.push_back({ "{", TokenType::LOOP, 0, 0 }); contexts.push_back({ C::LOOP, S::L_START }); return true;
					case '?': tokens.push_back({ "?", TokenType::QUESTION, 0, 0 }); return true;
					case '+': tokens.push_back({ "+", TokenType::PLUS, 0, 0 }); return true;
					case '*': tokens.push_back({ "*", TokenType::STAR, 0, 0 }); return true;
					case '|': tokens.push_back({ "|", TokenType::OR, 0, 0 }); return true;
					case '(': insertConcat(tokens); tokens.push_back({ "(", TokenType::OPENPAREN, 0, 0 }); return true;
					case ')': tokens.push_back({ ")", TokenType::CLOSEPAREN, 0, 0 }); return true;
					case '.': insertConcat(tokens); tokens.push_back({ ".", TokenType::DOT, 0, 0 }); return true;
					case '\\': insertConcat(tokens); contexts.push_back({ C::EXCHAR, S::E_START }); return true;
					default: insertConcat(tokens); tokens.push_back({ std::string(1, c) , TokenType::CHAR, c, 0 }); return true;
					}
				default: return false;
				}
			}

			bool ExcharProc(std::vector<Context>& contexts, char c, std::vector<Token>& tokens) {
				auto cur = contexts.rbegin();
				switch (cur->state) {
				case S::E_START:
					switch (c) {
					case 't': tokens.push_back({ "\t", TokenType::CHAR, 0, 0 }); contexts.pop_back(); return true;
					case 'n': tokens.push_back({ "\n", TokenType::CHAR, 0, 0 }); contexts.pop_back(); return true;
					case 'r': tokens.push_back({ "\r", TokenType::CHAR, 0, 0 }); contexts.pop_back(); return true;
					default: tokens.push_back({ "\\" + std::string(1, c), TokenType::CHAR, c, 0 }); contexts.pop_back(); return true;
					}
				default: return false;
				}
			}

			bool ClassProc(std::vector<Context>& contexts, char c, std::vector<Token>& tokens) {
				auto cur = contexts.rbegin();
				switch (cur->state) {
				case S::C_START:
					if (c == '^') {
						cur->state = S::C_MID; tokens.pop_back(); tokens.push_back({ "[^", TokenType::INVCLASS_OPEN, 0, 0 }); return true;
					}
				case S::C_MID:
					switch (c) {
					case ']': tokens.push_back({ "]", TokenType::CLASS_CLOSE, 0, 0 }); contexts.pop_back(); return true;
					case '-': tokens.push_back({ "-", TokenType::RANGE, 0, 0 }); return true;
					case '\\': insertSeperator(tokens); contexts.push_back({ C::EXCHAR, S::E_START }); return true;
					default: insertSeperator(tokens); tokens.push_back({ std::string(1, c), TokenType::CHAR, c, 0 }); return true;
					}
				default: return false;
				}
			}

			bool LoopProc(std::vector<Context>& contexts, char c, std::vector<Token>& tokens) {
				auto cur = contexts.rbegin();

				std::string prevStr;
				int prevData1 = 0;
				int prevData2 = 0;

				auto prevTok = tokens.rbegin();
				if (prevTok->type == TokenType::LOOP) {
					prevStr = prevTok->text;
					prevData1 = prevTok->data1;
					prevData2 = prevTok->data2;
				}

				switch (cur->state) {
				case S::L_START:
					switch (c) {
					case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
					case '9': cur->state = S::L_FIRST; tokens.pop_back(); tokens.push_back({ prevStr + std::string(1, c), TokenType::LOOP, prevData1 * 10 + (c - '0'), 0 }); return true;
					case ',': cur->state = S::L_VOID; tokens.pop_back();  tokens.push_back({ prevStr + std::string(1, c), TokenType::LOOP, prevData1, 0 }); return true;
					default: return false;
					}
				case S::L_FIRST:
					switch (c) {
					case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
					case '9': tokens.pop_back(); tokens.push_back({ prevStr + std::string(1, c), TokenType::LOOP, prevData1 * 10 + (c - '0'), 0 }); return true;
					case ',': cur->state = S::L_SECOND; tokens.pop_back();  tokens.push_back({ prevStr + std::string(1, c), TokenType::LOOP, prevData1, 0 }); return true;
					case '}': tokens.pop_back(); tokens.push_back({ prevStr + std::string(1, c), TokenType::LOOP, prevData1, prevData1 }); contexts.pop_back(); return true;
					default: return false;
					}
				case S::L_VOID:
					switch (c) {
					case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
					case '9': cur->state = S::L_SECOND; tokens.pop_back(); tokens.push_back({ prevStr + std::string(1, c), TokenType::LOOP, prevData1, prevData2 * 10 + (c - '0') }); return true;
					default: return false;
					}
				case S::L_SECOND:
					switch (c) {
					case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
					case '9': tokens.pop_back(); tokens.push_back({ prevStr + std::string(1, c), TokenType::LOOP, prevData1, prevData2 * 10 + (c - '0') }); return true;
					case '}': tokens.pop_back(); tokens.push_back({ prevStr + std::string(1, c), TokenType::LOOP, prevData1, prevData2 }); contexts.pop_back(); return true;
					default: return false;
					}
				default: return false;
				}
			}

			bool Lexer(std::string input, std::vector<Token>& tokens) {
				std::vector<Context> contexts;
				contexts.push_back({ C::NORMAL, S::N_START });

				for (auto it = input.begin(); it != input.end(); ++it) {
					char c = *it;

					switch (contexts.back().context) {
					case C::NORMAL: if (!NormalProc(contexts, c, tokens)) return false; break;
					case C::EXCHAR: if (!ExcharProc(contexts, c, tokens)) return false; break;
					case C::CLASS: if (!ClassProc(contexts, c, tokens)) return false; break;
					case C::LOOP: if (!LoopProc(contexts, c, tokens)) return false; break;
					}
				}

				return true;
			}
		}

		namespace postfixer {
			bool isOperator(TokenType type) {
				switch (type) {
				case TokenType::CLASS_SEPERATOR:
				case TokenType::OR:
				case TokenType::CONCAT:
				case TokenType::RANGE: return true;
				default: return false;
				}
			}

			unsigned int getPriority(TokenType type) {
				switch (type) {
				case TokenType::CLASS_SEPERATOR: return 3;
				case TokenType::OR: return 2;
				case TokenType::CONCAT: return 1;
				case TokenType::RANGE: return 0;
				default: return (unsigned int)-1;
				}
			}

			void Postfixer(std::vector<Token>& tokens) {
				std::vector<Token>input;
				std::stack<Token>stack;

				for (auto it = tokens.begin(); it != tokens.end(); ++it) {
					input.push_back(*it);
				}
				tokens.clear();

				for (auto it = input.begin(); it != input.end(); ++it) {
					if (isOperator(it->type)) {
						while (!stack.empty()) {
							if (getPriority(stack.top().type) > getPriority(it->type))
								break;
							tokens.push_back(stack.top()); stack.pop();
						}
						if (it->type != TokenType::CLASS_SEPERATOR)
							stack.push(*it);
					}
					else if (it->type == TokenType::CLASS_OPEN || it->type == TokenType::INVCLASS_OPEN) {
						tokens.push_back(*it);
						stack.push(*it);
					}
					else if (it->type == TokenType::CLASS_CLOSE) {
						while (!stack.empty()) {
							if (stack.top().type == TokenType::CLASS_OPEN || stack.top().type == TokenType::INVCLASS_OPEN) {
								stack.pop();
								break;
							}
							tokens.push_back(stack.top()); stack.pop();
						}
						tokens.push_back(*it);
					}
					else if (it->type == TokenType::OPENPAREN) {
						stack.push(*it);
					}
					else if (it->type == TokenType::CLOSEPAREN) {
						while (!stack.empty()) {
							if (stack.top().type == TokenType::OPENPAREN) {
								stack.pop();
								break;
							}
							tokens.push_back(stack.top()); stack.pop();
						}
					}
					else {
						tokens.push_back(*it);
					}
				}
				while (!stack.empty()) {
					tokens.push_back(stack.top());
					stack.pop();
				}
			}
		}

		namespace converter {
			using namespace graph;
			using namespace graph::generate;
			using namespace regex;

			namespace functions {
				Graph gen_dot(void) {
					Graph graph;
					std::bitset<128> symbols; symbols.set();
					graph = createClass(symbols);

					return graph;
				}

				Graph gen_char(char c) {
					Graph graph = create(c);
					return graph;
				}

				Graph gen_class(std::vector<char>& c) {
					Graph graph;
					std::bitset<128> symbols; symbols.reset();
					for (auto it = c.begin(); it != c.end(); ++it)
						symbols.set((unsigned char)*it);
					graph = createClass(symbols);

					return graph;
				}

				Graph gen_invclass(std::vector<char>& c) {
					Graph graph;
					std::bitset<128> symbols; symbols.set();
					for (auto it = c.begin(); it != c.end(); ++it)
						symbols.reset((unsigned char)*it);
					graph = createClass(symbols);

					return graph;
				}

				Graph gen_loop(Graph& A, int min, int max) {
					Graph epsilon = createEpsilon();

					Graph graph;
					graph.start = 0; graph.end.insert(0); graph.set.genID(0);

					for (int i = 0; i < min; i++) {
						graph = chain(graph, A);
					}
					for (int i = 0; i < max - min; i++) {
						graph = chain(graph, junction(A, epsilon));
					}

					return graph;
				}

				Graph gen_question(Graph& A) {
					Graph epsilon = createEpsilon();

					Graph graph;
					graph = junction(A, epsilon);

					return graph;
				}

				Graph gen_plus(Graph& A) {
					Graph graph = A;
					for (auto it = graph.end.begin(); it != graph.end.end(); ++it)
						graph.transitions.push_back({ *it, graph.start, 0, true });

					return graph;
				}

				Graph gen_star(Graph& A) {
					Graph graph = A;
					for (auto it = graph.end.begin(); it != graph.end.end(); ++it) {
						graph.transitions.push_back({ *it, graph.start, 0, true });
						graph.transitions.push_back({ graph.start, *it, 0, true });
					}

					return graph;
				}

				Graph gen_or(Graph& A, Graph& B) {
					return junction(A, B);
				}

				Graph gen_concat(Graph& A, Graph& B) {
					return chain(A, B);
				}
			}

			int CreateNFA(std::vector<Token>& postfix, Graph& NFA) {
				using namespace functions;

				class EqData {
				public:
					enum Type {
						GRAPH,
						TOKEN
					};

					Type type;

					Graph graph;
					Token token;

					EqData(Token input) {
						this->type = Type::TOKEN;
						this->token = input;
						this->graph = {};
					}

					EqData(const Graph& input) {
						this->type = Type::GRAPH;
						this->graph = input;
					}

					~EqData(void) {
					}

					bool convert2graph(void) {
						if (this->type == GRAPH) return true;
						if (this->token.type != TokenType::CHAR) return false;

						this->type = GRAPH;
						this->graph = gen_char(this->token.data1);
						return true;
					}
				};

				std::stack<EqData> stack;
				for (auto it = postfix.begin(); it != postfix.end(); ++it) {
					switch (it->type) {
					case TokenType::CLASS_OPEN:
					case TokenType::INVCLASS_OPEN:
					case TokenType::CHAR: {
						stack.push(EqData(*it));
						break;
					}
					case TokenType::DOT: {
						stack.push(EqData(gen_dot()));
						break;
					}
					case TokenType::CLASS_CLOSE: {
						std::vector<char> list;

						while (!stack.empty()) {
							if (stack.top().type != EqData::TOKEN) return -1;

							if (stack.top().token.type == TokenType::CLASS_OPEN) {
								stack.pop();
								goto CLASS_OPEN_PROC;
							}
							else if (stack.top().token.type == TokenType::INVCLASS_OPEN) {
								stack.pop();
								goto INVCLASS_OPEN_PROC;
							}
							else if (stack.top().token.type != TokenType::CHAR) return -2;

							list.push_back(stack.top().token.data1);
							stack.pop();
						}
						return -3;
					CLASS_OPEN_PROC:
						stack.push(EqData(gen_class(list)));
						break;
					INVCLASS_OPEN_PROC:
						stack.push(EqData(gen_invclass(list)));
						break;
					}
					case TokenType::RANGE: {
						if (stack.empty()) return -4; EqData to = stack.top();  stack.pop(); if (to.type != EqData::TOKEN) return -5;
						if (stack.empty()) return -6; EqData from = stack.top();  stack.pop(); if (from.type != EqData::TOKEN) return -7;

						for (unsigned char i = from.token.data1; i <= to.token.data1; i++) {
							stack.push(EqData({ "", TokenType::CHAR, i, 0 }));
						}

						break;
					}
					case TokenType::LOOP: {
						if (stack.empty()) return -8; EqData prev = stack.top();  stack.pop(); if (!prev.convert2graph()) return -9;

						stack.push(EqData(gen_loop(prev.graph, it->data1, it->data2)));
						break;
					}
					case TokenType::QUESTION: {
						if (stack.empty()) return -10; EqData prev = stack.top();  stack.pop(); if (!prev.convert2graph()) return -11;

						stack.push(EqData(gen_question(prev.graph)));
						break;
					}
					case TokenType::PLUS: {
						if (stack.empty()) return -12; EqData prev = stack.top();  stack.pop(); if (!prev.convert2graph()) return -13;

						stack.push(EqData(gen_plus(prev.graph)));
						break;
					}
					case TokenType::STAR: {
						if (stack.empty()) return -14; EqData prev = stack.top();  stack.pop(); if (!prev.convert2graph()) return -15;

						stack.push(EqData(gen_star(prev.graph)));
						break;
					}
					case TokenType::OR: {
						if (stack.empty()) return -16; EqData second = stack.top();  stack.pop(); if (!second.convert2graph()) return -17;
						if (stack.empty()) return -18; EqData first = stack.top();  stack.pop(); if (!first.convert2graph()) return -19;

						stack.push(EqData(gen_or(first.graph, second.graph)));
						break;
					}
					case TokenType::CONCAT: {
						if (stack.empty()) return -20; EqData second = stack.top(); stack.pop(); if (!second.convert2graph()) return -21;
						if (stack.empty()) return -22; EqData first = stack.top(); stack.pop(); if (!first.convert2graph()) return -23;

						stack.push(EqData(gen_concat(first.graph, second.graph)));
						break;
					}
					default: return -24;
					}
				}
				if (stack.empty())
					return -25;
				EqData fin = stack.top();
				if (!fin.convert2graph()) return -26;
				NFA = fin.graph;

				return 0;
			}

			namespace convertDfaUtil {
				using u64 = unsigned __int64;

				inline void getTransitionData(
					std::unordered_multimap<ID, ID>& revEpsilonMap,
					std::unordered_map<ID, std::multimap<char, ID>>& transitionMap,
					std::unordered_map<ID, int>& epsilonLeft,
					std::unordered_set<ID>& noTransition,
					Graph& graph) {

					graph.set.getAll(noTransition);
					for (auto it = noTransition.begin(); it != noTransition.end(); ++it)
						epsilonLeft[*it] = 0;

					for (auto it = graph.transitions.begin(); it != graph.transitions.end(); ++it) {
						if (it->epsilon) {
							revEpsilonMap.insert({ it->to, it->from });
							epsilonLeft[it->from]++;
						}
						else {
							for (int i = 0; i < 128; i++) {
								if (it->symbols.test(i))
									transitionMap[it->from].insert({ i, it->to });
							}
							noTransition.erase(it->from);
						}
					}
				}

				inline ID findRoot(std::vector<ID>& parent, ID x) {
					if (parent[x] == x)
						return x;
					return parent[x] = findRoot(parent, parent[x]);
				}

				inline void unionGroup(std::vector<size_t>& size, std::vector<ID>& parent, ID a, ID b) {
					ID rootA = findRoot(parent, a), rootB = findRoot(parent, b);
					if (size[rootA] < size[rootB]) {
						parent[rootA] = rootB;
						size[rootB] += size[rootA];
					}
					else {
						parent[rootB] = rootA;
						size[rootA] += size[rootB];
					}
				}

				inline void getBitsMaps(
					std::unordered_map<ID, std::pair<u64, u64>>& compressMap,
					std::unordered_map<u64, ID>& decompressMap,
					std::unordered_multimap<ID, ID>& epsilonMap,
					std::unordered_set<ID>& endSet,
					Graph& graph) {

					ID maxID = graph.set.getMax();

					std::unordered_map<ID, std::vector<ID>> ends;

					std::vector<size_t> size;
					std::vector<ID> parent;
					size.resize(maxID + 1, 1);
					parent.resize(maxID + 1);

					int ct = 0;
					for (auto it = parent.begin(); it != parent.end(); ++it)
						*it = ct++;

					for (auto it = epsilonMap.begin(); it != epsilonMap.end(); ++it)
						unionGroup(size, parent, it->first, it->second);

					for (auto it = endSet.begin(); it != endSet.end(); ++it)
						ends[findRoot(parent, *it)].push_back(*it);

					ct = 0;
					for (auto it = ends.begin(); it != ends.end(); ++it) {
						int id = 0;
						for (auto si = it->second.begin(); si != it->second.end(); ++si) {
							compressMap[*si] = { id, ct };
							decompressMap[id + ct] = *si;
							id++;
						}
						ct += id;
					}
				}

				typedef struct _TarjanDFSdata {
					std::unordered_map<ID, ID>& index;
					std::unordered_map<ID, ID>& low;
					std::unordered_map<ID, bool>& onStack;
					std::unordered_multimap<ID, ID>& transitionMap;
					int indexCt;
				} TarjanDFSdata;

				inline void tarjanDFS(TarjanDFSdata& data, ID id) {
					data.index[id] = data.indexCt++;
					data.low[id] = data.index[id];
					data.onStack[id] = true;

					auto range = data.transitionMap.equal_range(id);

					for (auto it = range.first; it != range.second; ++it) {
						if (!data.index[it->second]) {
							tarjanDFS(data, it->second);
							data.low[id] = std::min(data.low[id], data.low[it->second]);
							continue;
						}

						if (data.onStack[it->second])
							data.low[id] = std::min(data.low[id], data.index[it->second]);
					}

					data.onStack[id] = false;
				}

				inline void tarjan(
					std::unordered_map<ID, ID>& sccMap,
					std::unordered_multimap<ID, ID>& transitionMap) {

					std::unordered_map<ID, ID> index, low;
					std::unordered_map<ID, bool> onStack;

					TarjanDFSdata data = { index, low, onStack, transitionMap, 1};

					std::unordered_set<ID> idSet;
					for (auto it = transitionMap.begin(); it != transitionMap.end(); ++it) {
						idSet.insert(it->first);
						idSet.insert(it->second);
					}

					for (auto it = idSet.begin(); it != idSet.end(); ++it) {
						if (index[*it])
							continue;

						tarjanDFS(data, *it);
						for (auto si = onStack.begin(); si != onStack.end(); ++si)
							si->second = false;
					}

					std::unordered_map<ID, ID> rIndex;
					for (auto it = index.begin(); it != index.end(); ++it)
						rIndex[it->second] = it->first;

					for (auto it = idSet.begin(); it != idSet.end(); ++it) {
						sccMap[*it] = rIndex[low[*it]];
					}
				}

				inline ID replaceScc(ID id, std::unordered_map<ID, ID>& sccMap) {
					auto it = sccMap.find(id);
					if (it == sccMap.end())
						return id;
					return it->second;
				}

				inline void replace(Graph& out, const Graph& graph, std::unordered_map<ID, ID>& sccMap) {
					out.start = replaceScc(graph.start, sccMap);
					for (auto it = graph.end.begin(); it != graph.end.end(); ++it)
						out.end.insert(replaceScc(*it, sccMap));

					std::set<std::pair<ID, ID>> tmpEpsilonSet;
					std::map<std::pair<ID, ID>, std::bitset<128>> tmpTransitionMap;
					for (auto it = graph.transitions.begin(); it != graph.transitions.end(); ++it) {
						ID from = replaceScc(it->from, sccMap), to = replaceScc(it->to, sccMap);
						if (it->epsilon) {
							if (from == to)
								continue;
							tmpEpsilonSet.insert({ from, to });
						}
						else {
							tmpTransitionMap[{ from, to }] |= it->symbols;
						}
					}

					for (auto it = tmpEpsilonSet.begin(); it != tmpEpsilonSet.end(); ++it)
						out.transitions.push_back({ it->first, it->second, std::bitset<128>(), true });
					for (auto it = tmpTransitionMap.begin(); it != tmpTransitionMap.end(); ++it)
						out.transitions.push_back({ it->first.first, it->first.second, it->second, false });

					std::vector<ID> tmpSet;
					graph.set.getAll(tmpSet);
					for (auto it = tmpSet.begin(); it != tmpSet.end(); ++it)
						out.set.genID(replaceScc(*it, sccMap));
				}
			}

			int ConvertDFA(Graph& graph, std::unordered_multimap<ID, ID>& endMap) {
				using namespace convertDfaUtil;
				Graph out;

				std::unordered_multimap<ID, ID> epsilonMap;
				for (auto it = graph.transitions.begin(); it != graph.transitions.end(); ++it) {
					if (it->epsilon)
						epsilonMap.insert({ it->from, it->to });
				}

				std::unordered_map<ID, ID> sccMap;
				Graph tGraph;

				tarjan(sccMap, epsilonMap);
				replace(tGraph, graph, sccMap);

				std::unordered_multimap<ID, ID> revEpsilonMap;
				std::unordered_map<ID, std::multimap<char, ID>> transitionMap;
				std::unordered_map<ID, int> epsilonLeft;
				std::unordered_set<ID> noTransition;

				getTransitionData(revEpsilonMap, transitionMap, epsilonLeft, noTransition, tGraph);

				std::unordered_set<ID> epsilonEndSet;
				for (auto it = transitionMap.begin(); it != transitionMap.end(); ++it)
					epsilonEndSet.insert(it->first);
				for (auto it = tGraph.end.begin(); it != tGraph.end.end(); ++it)
					epsilonEndSet.insert(*it);

				std::unordered_map<ID, std::pair<u64, u64>> compressMap;
				std::unordered_map<u64, ID> decompressMap;
				std::unordered_map<ID, std::pair<std::vector<u64>, u64>> bitsMap;

				u64 maxID = 0;

				getBitsMaps(compressMap, decompressMap, revEpsilonMap, epsilonEndSet, tGraph);

				typedef struct _EQData {
					ID cur, prev;
					u64 shift;
				} EQData;

				std::queue<EQData> epsilonQ;

				for (auto it = epsilonEndSet.begin(); it != epsilonEndSet.end(); ++it) {
					std::pair<u64, u64>& comp = compressMap[*it];
					epsilonQ.push({ *it, *it, comp.second });
					std::vector<u64> arr((comp.first >> 6) + 1, 0);
					arr[comp.first >> 6] |= u64(1) << (comp.first & 0b111111);
					bitsMap[*it] = { arr, comp.second };
					epsilonLeft[*it]++;
				}

				while (!epsilonQ.empty()) {
					EQData cur = epsilonQ.front(); epsilonQ.pop();

					size_t curSize = bitsMap[cur.cur].first.size(), prevSize = bitsMap[cur.prev].first.size();
					if (curSize < prevSize)
						bitsMap[cur.cur].first.resize(prevSize);

					typename std::vector<u64>::iterator curPt = bitsMap[cur.cur].first.begin(), prevPt = bitsMap[cur.prev].first.begin();
					for (curPt, prevPt; prevPt != bitsMap[cur.prev].first.end(); ++prevPt)
						*(curPt++) |= *prevPt;
					bitsMap[cur.cur].second = cur.shift;

					auto range = revEpsilonMap.equal_range(cur.cur);
					if (--epsilonLeft[cur.cur] != 0)
						continue;
					for (auto it = range.first; it != range.second; ++it) {
						epsilonQ.push({ it->second, cur.cur, cur.shift });
					}
				}
					 
				/*
				std::cout << "sccMap:" << std::endl;
				for (auto it = sccMap.begin(); it != sccMap.end(); ++it) {
					std::cout << it->first << ": " << it->second << std::endl;
				}

				std::cout << "transitionMap:" << std::endl;
				for (auto it = transitionMap.begin(); it != transitionMap.end(); ++it) {
					std::cout << it->first << ": ";
					for (auto si = it->second.begin(); si != it->second.end(); ++si)
						std::cout << "[" << si->first << ", " << si->second << "], ";
					std::cout << std::endl;
				}

				std::cout << "epsilonMap:" << std::endl;
				for (auto it = revEpsilonMap.begin(); it != revEpsilonMap.end(); ++it) {
					std::cout << it->second << ": " << it->first << std::endl;
				}

				std::cout << "epsilonEndSet:" << std::endl;
				for (auto it = epsilonEndSet.begin(); it != epsilonEndSet.end(); ++it) {
					std::cout << *it << std::endl;
				}

				std::cout << "compressMap:" << std::endl;
				for (auto it = compressMap.begin(); it != compressMap.end(); ++it) {
					std::cout << it->first << ": " << it->second.first << ", " << it->second.second << std::endl;
				}

				std::cout << "decompressMap:" << std::endl;
				for (auto it = decompressMap.begin(); it != decompressMap.end(); ++it) {
					std::cout << it->first << ": " << it->second << std::endl;
				}

				std::cout << "bitsMap:" << std::endl;
				for (auto it = bitsMap.begin(); it != bitsMap.end(); ++it) {
					std::cout << it->first << ": ";
					for (auto si = it->second.first.rbegin(); si != it->second.first.rend(); ++si) {
						if (*si)
							std::cout << std::hex << *si << std::dec;
						else {
							if (si != it->second.first.rbegin())
								std::cout << "0000000000000000";
						}
					}
					std::cout << ", " << it->second.second << std::endl;
				}*/

				typedef struct _LessIDbit {
					bool operator() (const std::map<u64, std::vector<u64>>& left, const std::map<u64, std::vector<u64>>& right) const {
						auto leftIt = left.rbegin(), rightIt = right.rbegin();

						while (true) {
							if (leftIt == left.rend()) {
								if (rightIt == right.rend())
									return false;
								return true;
							}
							if (rightIt == right.rend())
								return false;

							if (leftIt->first < rightIt->first)
								return true;
							if (leftIt->first > rightIt->first)
								return false;

							if (leftIt->second.size() < rightIt->second.size())
								return true;
							if (leftIt->second.size() > rightIt->second.size())
								return false;

							for (u64 i = 0; i < leftIt->second.size(); i++) {
								if (leftIt->second[i] < rightIt->second[i])
									return true;
								if (leftIt->second[i] > rightIt->second[i])
									return false;
							}

							++leftIt; ++rightIt;
						}
					}
				} LessIDbit;

				ID setId = 0;
				std::map<std::map<u64, std::vector<u64>>, ID, LessIDbit> subsetMap;
				std::unordered_map<ID, std::unordered_map<ID, std::bitset<128>>> subsetTransitionMap;

				typedef struct _SQData {
					std::map<u64, std::vector<u64>> cur;
					ID prev;
				} SQData;

				std::queue<SQData> subsetQ;

				ID startID = setId++;

				std::map<u64, std::vector<u64>> startBits = {};
				u64 startShift = bitsMap[tGraph.start].second;
				for (auto it = bitsMap[tGraph.start].first.begin(); it != bitsMap[tGraph.start].first.end(); ++it)
					startBits[startShift].push_back(*it);

				subsetMap[startBits] = startID;
				subsetQ.push({ startBits, startID });

				while (!subsetQ.empty()) {
					SQData cur = subsetQ.front(); subsetQ.pop();

					std::unordered_map<char, std::unordered_set<ID>> gotoMap;
					for (auto it = cur.cur.begin(); it != cur.cur.end(); ++it) {
						for (u64 i = 0; i < it->second.size(); i++) {
							u64 bit = it->second[i];
							for (int j = 0; j < 64 && bit; j++) {
								if (bit & 0b1) {
									ID curTrueID = decompressMap[(i << 6) + j + it->first];
									for (auto it = transitionMap[curTrueID].begin(); it != transitionMap[curTrueID].end(); ++it)
										gotoMap[it->first].insert(it->second);
								}
								bit >>= 1;
							}
						}
					}

					for (auto it = gotoMap.begin(); it != gotoMap.end(); ++it) {
						std::map<u64, std::vector<u64>> set;
						for (auto si = it->second.begin(); si != it->second.end(); ++si) {
							std::pair<std::vector<u64>, u64> tmp = bitsMap[*si];
							set[tmp.second].resize(std::max(set[tmp.second].size(), tmp.first.size()), 0);

							auto bi = set[tmp.second].begin();
							for (auto it = tmp.first.begin(); it != tmp.first.end(); ++it)
								*(bi++) |= *it;
						}

						bool found = (subsetMap.find(set) != subsetMap.end());
						if (!found)
							subsetMap[set] = setId++;

						ID curID = subsetMap[set];
						subsetTransitionMap[cur.prev][curID].set(it->first);

						if (found)
							continue;

						subsetQ.push({ set, curID });
					}
				}

				std::unordered_map<u64, std::unordered_set<u64>> endSet;
				for (auto it = tGraph.end.begin(); it != tGraph.end.end(); ++it) {
					std::pair<u64, u64>& comp = compressMap[*it];
					endSet[comp.second].insert(comp.first);
				}

				std::unordered_multimap<ID, ID> rSccMap;
				for (auto it = sccMap.begin(); it != sccMap.end(); ++it)
					rSccMap.insert({ it->second, it->first });

				out.start = 0;

				for (ID i = 0; i < setId; i++)
					out.set.genID(i);

				for (auto it = subsetMap.begin(); it != subsetMap.end(); ++it) {
					for (auto si = endSet.begin(); si != endSet.end(); ++si) {
						if (it->first.find(si->first) == it->first.end())
							continue;

						const std::vector<u64>& vec = it->first.find(si->first)->second;
						size_t size = vec.size();

						for (auto ti = si->second.begin(); ti != si->second.end(); ++ti) {
							if (*ti >= (size << 6))
								continue;
							if (vec[*ti >> 6] & (u64(1) << (*ti & 0b111111))) {
								out.end.insert(it->second);

								ID id = decompressMap[*ti + si->first];
								auto range = rSccMap.equal_range(id);

								if (range.first == range.second)
									endMap.insert({ it->second, id });
								for (auto qi = range.first; qi != range.second; ++qi)
									endMap.insert({ it->second, qi->second });

								break;
							}
						}
					}
				}

				for (auto it = subsetTransitionMap.begin(); it != subsetTransitionMap.end(); ++it) {
					for (auto si = it->second.begin(); si != it->second.end(); ++si)
						out.transitions.push_back({ it->first, si->first, si->second, false });
				}

				graph = out;

				return 0;
			}
		}
	}

	namespace table {
		using namespace graph;

		typedef struct _DFATable {
			ID** table;
			size_t size;
		} DFATable;

		DFATable createTable(Graph& DFA) {
			size_t size = DFA.set.getMax() + 1;
			ID** table = new ID * [size];
			for (ID i = 0; i < size; i++) {
				table[i] = new ID[128];
				for (int j = 0; j < 128; j++)
					table[i][j] = (ID)-1;
			}

			for (auto it = DFA.transitions.begin(); it != DFA.transitions.end(); ++it) {
				for (int i = 0; i < 128; i++) {
					if (it->symbols.test(i))
						table[it->from][i] = it->to;
				}
			}

			return DFATable{ table, size };
		}

		void copyTable(DFATable& dest, const DFATable& src) {
			dest.size = src.size;
			dest.table = new ID * [src.size];
			for (ID i = 0; i < src.size; i++) {
				dest.table[i] = new ID[128];
				for (int j = 0; j < 128; j++)
					dest.table[i][j] = src.table[i][j];
			}
		}

		std::string int2str(int num, int len) {
			std::string out;
			std::vector<char> buff;
			for (int i = 0; i < len; i++) {
				buff.push_back((num % 10) + '0');
				num /= 10;
			}
			for (auto it = buff.rbegin(); it != buff.rend(); ++it)
				out.push_back(*it);

			return out;
		}

		std::string space(int len) {
			std::string out;
			for (int i = 0; i < len; i++)
				out.push_back(' ');
			return out;
		}

		void print(DFATable& table, int len) {
			std::cout << space(len + 2);
			for (int i = 32; i < 127; i++)
				std::cout << (char)i << space(len);
			std::cout << std::endl;
			for (int i = 0; i < table.size; i++) {
				std::cout << int2str(i, len) << ": ";
				for (int j = 32; j < 127; j++) {
					if (table.table[i][j] == (unsigned __int64)-1)
						std::cout << "." << space(len);
					else
						std::cout << int2str(table.table[i][j], len) << " ";
				}
				std::cout << std::endl;
			}
		}
	}

	template<class TokenType>
	class LexerFactory;

	template<class TokenType>
	class Lexer {
	public:
		friend class LexerFactory<TokenType>;

	private:
		using u64 = unsigned __int64;
		using ID = graph::ID;
		using Table = table::DFATable;

		typedef struct _EndData {
			TokenType token;
			u64 priority;
		} EndData;

		Table dfaTable;
		std::unordered_multimap<ID, EndData> endMap;

	public:
		typedef struct _Token {
			std::string text;
			TokenType type;

			inline operator TokenType() const {
				return this->type;
			}

			inline operator u64() const {
				return (u64)this->type;
			}
		} Token;

		void lex(std::vector<Token>& tokens, std::string text) {
			text.push_back(0);

			typedef struct _TokenData {
				TokenType type;
				size_t len;
				std::string::iterator it;
				u64 priority;
			} TokenData;

			ID state = 0;
			std::string curString;
			TokenData endPoint = { TokenType(-1), 0, text.begin(), 0 };
			for (auto it = text.begin(); it != text.end(); ++it) {
				if (*it == 0)
					goto createToken;

				curString.push_back(*it);
				state = this->dfaTable.table[state][*it];

				if (state == (ID)-1) {
				createToken:
					if (!curString.size())
						break;
					if (endPoint.type != TokenType(-1))
						curString.resize(endPoint.len);
					else
						endPoint = { TokenType(-1), 0, it, 0 };

					tokens.push_back({ curString, endPoint.type });

					curString.clear();
					it = endPoint.it;

					endPoint = { TokenType(-1), 0, it, 0 };
					state = 0;

					continue;
				}
				if (this->endMap.find(state) != this->endMap.end()) {
					auto range = this->endMap.equal_range(state);
					for (auto si = range.first; si != range.second; ++si) {
						if (endPoint.len < curString.size() || (endPoint.len == curString.size() && endPoint.priority > si->second.priority))
							endPoint = { si->second.token, curString.size(), it, si->second.priority };
					}
				}
			}
		}
	};

	using u8 = unsigned __int8;

	template<class TokenType>
	class LexerFactory {
	public:
		using CreateData = std::vector<std::vector<std::pair<std::string, TokenType>>>;

		typedef struct _UpdateData {
			table::DFATable table;
			std::vector<std::pair<graph::ID, typename Lexer<TokenType>::EndData>> endData;
		} UpdateData;
	private:
		using u64 = unsigned __int64;
		using ID = graph::ID;
		using Table = table::DFATable;

		typedef struct _Rule {
			std::string regex;
			TokenType token;
			u64 priority;
		} Rule;

		using Graph = graph::Graph;
		using EndData = typename Lexer<TokenType>::EndData;

		std::vector<Rule> rules;
		std::unordered_multimap<ID, EndData> endDatas;
		Table dfaTable;

		void clearTable(void) {
			if (this->dfaTable.table != NULL) {
				for (size_t i = 0; i < this->dfaTable.size; i++)
					delete[] this->dfaTable.table[i];
				delete[] this->dfaTable.table;
			}
			this->dfaTable.size = 0;
			this->dfaTable.table = NULL;
		}

	public:
		LexerFactory(void) {
			this->rules.clear();
			this->dfaTable.size = 0;
			this->dfaTable.table = NULL;
		}

		~LexerFactory() {
			this->clearTable();
		}

		void setRules(const CreateData& data) {
			this->rules.clear();

			u64 priority = 0;
			for (auto group = data.begin(); group != data.end(); ++group) {
				for (auto rule = group->begin(); rule != group->end(); ++rule)
					this->rules.push_back({ rule->first, rule->second, priority });
				priority++;
			}
		}

		void update(void) {
			this->clearTable();

			std::unordered_map<ID, std::unordered_map<ID, EndData>> NFAendDatas;
			std::vector<Graph> NFAs;
			ID graphID = 0;
			for (auto it = this->rules.begin(); it != this->rules.end(); ++it) {
				std::vector<regex::Token> tokens;
				regex::lexer::Lexer(it->regex, tokens);
				regex::postfixer::Postfixer(tokens);
				Graph NFA;
				regex::converter::CreateNFA(tokens, NFA);

				NFAs.push_back(NFA);
				for (auto si = NFA.end.begin(); si != NFA.end.end(); ++si)
					NFAendDatas[graphID][*si] = { it->token, it->priority };

				graphID++;
			}

			std::unordered_map<ID, std::pair<ID, ID>> nfaEndMap;
			std::unordered_multimap<ID, ID> dfaEndMap;

			Graph DFA = graph::generate::split(NFAs, nfaEndMap);
			regex::converter::ConvertDFA(DFA, dfaEndMap);

			for (auto it = dfaEndMap.begin(); it != dfaEndMap.end();) {
				if (nfaEndMap.find(it->second) == nfaEndMap.end())
					it = dfaEndMap.erase(it);
				else
					++it;
			}

			for (auto it = dfaEndMap.begin(); it != dfaEndMap.end(); ++it) {
				EndData tmp = NFAendDatas[nfaEndMap[it->second].first][nfaEndMap[it->second].second];
				this->endDatas.insert({ it->first, tmp });
			}

			this->dfaTable = table::createTable(DFA);
		}

		void getData(UpdateData& out) {
			table::copyTable(out.table, this->dfaTable);
			for (auto it = this->endDatas.begin(); it != this->endDatas.end(); ++it)
				out.endData.push_back({ it->first, it->second });
		}

		void directUpdate(const UpdateData& in) {
			this->clearTable();

			table::copyTable(this->dfaTable, in.table);
			for (auto it = in.endData.cbegin(); it != in.endData.cend(); ++it)
				this->endDatas.insert({ it->first, it->second });
		}

		Lexer<TokenType> create(void) {
			Lexer<TokenType> out;
			out.dfaTable = this->dfaTable;
			for (auto it = this->endDatas.begin(); it != this->endDatas.end(); ++it)
				out.endMap.insert({ it->first, it->second });

			return out;
		}
	};
}

#endif