#ifndef DPICP_ASSEMBLER
#define DPICP_ASSEMBLER

#include <iostream>
#include <fstream>

#include <string>
#include <vector>
#include <array>
#include <stack>
#include <queue>
#include <map>
#include <set>

#include "lexer.hpp"
#include "preprocesser.hpp"
#include "parser.hpp"
#include "evaluator.hpp"

#include "iotool.hpp"

namespace assembler {
	namespace lexer {
		using u64 = unsigned __int64;

		TOKENSTART(TokenType)
			add, addc, addi,
			sub, subc, subi,
			bxr, bxri,
			bor, bori,
			bnd, bndi,
			shiftl, shiftlc, shiftli,
			shiftr, shiftrc, shiftri,
			rol, roli, ror, rori,
			cmp, cmpi, test, testi,

			mov, set, sets,
			push, pop,

			ld, st,

			jmp, ijmp,
			call, ret,
			nop, halt,

			reg,
			gen,
			sbp, zero, one, full, pc, stack, flag,

			neg, pos, carry, carry4, overflow,

			_32B_, _16L_, _8L_, _8H_, _S16H_, _S16L_, _S8L_, _S8H_,

			text, data, bss,

			define,
			macro,
			end,

			macro_arg,
			identifier_unique,

			dot,
			comma,
			hash,

			openparen, closeparen,
			openbrace, closebrace,
			openbrack, closebrack,

			hexnum, decnum, octnum, binnum,

			colon, semicolon,

			plus, minus,
			star, dstar, slash, percent,
			tilde, ampersend, verticalbar, caret,
			leftshift, rightshift,

			comment,

			whitespace,
			newline,

			string,
			identifier,

			TOKENEND();

		using LexerFactory = lexer_generator::LexerFactory<TokenType>;
		using LFCD = LexerFactory::CreateData;
		using Lexer = lexer_generator::Lexer<TokenType>;
		using Token = Lexer::Token;

		const LFCD CreateData = {
			{
				{ "add", TokenType::add },
				{ "addc", TokenType::addc },
				{ "addi", TokenType::addi },
				{ "sub", TokenType::sub },
				{ "subc", TokenType::subc },
				{ "subi", TokenType::subi },
				{ "bxr", TokenType::bxr },
				{ "bxri", TokenType::bxri },
				{ "bor", TokenType::bor },
				{ "bori", TokenType::bori },
				{ "bnd", TokenType::bnd },
				{ "bndi", TokenType::bndi },
				{ "shiftl", TokenType::shiftl },
				{ "shiftlc", TokenType::shiftlc },
				{ "shiftli", TokenType::shiftli },
				{ "shiftr", TokenType::shiftr },
				{ "shiftrc", TokenType::shiftrc },
				{ "shiftri", TokenType::shiftri },
				{ "rol", TokenType::rol },
				{ "roli", TokenType::roli},
				{ "ror", TokenType::ror},
				{ "rori", TokenType::rori},
				{ "cmp", TokenType::cmp },
				{ "cmpi", TokenType::cmpi },
				{ "test", TokenType::test },
				{ "testi", TokenType::testi },
				{ "set", TokenType::set },
				{ "sets", TokenType::sets },
				{ "mov", TokenType::mov },
				{ "push", TokenType::push },
				{ "pop", TokenType::pop },
				{ "ld", TokenType::ld },
				{ "st", TokenType::st },
				{ "jmp", TokenType::jmp },
				{ "ijmp", TokenType::ijmp },
				{ "call", TokenType::call },
				{ "ret", TokenType::ret },
				{ "nop", TokenType::nop },
				{ "halt", TokenType::halt },

				{ "reg",TokenType::reg },
				{ "gen", TokenType::gen },
				{ "sbp", TokenType::sbp },
				{ "zero", TokenType::zero },
				{ "one", TokenType::one },
				{ "full", TokenType::full },
				{ "pc", TokenType::pc },
				{ "stack", TokenType::stack },
				{ "flag", TokenType::flag },

				{ "neg", TokenType::neg },
				{ "pos", TokenType::pos },
				{ "carry", TokenType::carry },
				{ "carry4", TokenType::carry4 },
				{ "overflow", TokenType::overflow },

				{ "32[bB]", TokenType::_32B_ },
				{ "16[lL]", TokenType::_16L_ },
				{ "8[lL]", TokenType::_8L_ },
				{ "8[hH]", TokenType::_8H_ },
				{ "[sS]16[hH]", TokenType::_S16H_ },
				{ "[sS]16[lL]", TokenType::_S16L_ },
				{ "[sS]8[lL]", TokenType::_S8L_ },
				{ "[sS]8[hH]", TokenType::_S8H_ },

				{ "\\.text", TokenType::text },
				{ "\\.data", TokenType::data },
				{ "\\.bss", TokenType::bss },

				{ "#define", TokenType::define },
				{ "#macro", TokenType::macro },
				{ "#end", TokenType::end },

				{ "\\.", TokenType::dot },
				{ ",", TokenType::comma },
				{ "#", TokenType::hash },
				{ "\\(", TokenType::openparen },
				{ "\\)", TokenType::closeparen },
				{ "\\{", TokenType::openbrace },
				{ "\\}", TokenType::closebrace },
				{ "\\[", TokenType::openbrack },
				{ "\\]", TokenType::closebrack },

				{ ":", TokenType::colon },
				{ ";", TokenType::semicolon },

				{ "\\+", TokenType::plus },
				{ "-", TokenType::minus },
				{ "\\*\\*", TokenType::dstar },
				{ "\\*", TokenType::star },
				{ "/", TokenType::slash },
				{ "%", TokenType::percent },
				{ "~", TokenType::tilde },
				{ "&", TokenType::ampersend },
				{ "\\|", TokenType::verticalbar },
				{ "^", TokenType::caret },
				{ "<<", TokenType::leftshift },
				{ ">>", TokenType::rightshift }
			},
			{
				{ "0x[0-9a-fA-F]+", TokenType::hexnum },
				{ "[0-9]+", TokenType::decnum },
				{ "0o[0-7]+", TokenType::octnum },
				{ "0b[01]+", TokenType::binnum },

				{ "#[0-9]+", TokenType::macro_arg },

				{ "[a-zA-Z_][a-zA-Z0-9_]*", TokenType::identifier },
				{ "##[a-zA-Z_][a-zA-Z0-9_]*", TokenType::identifier_unique },
				{ "\"[^\n\"]*([^\n\"]+(\\\\[\n\"])+)*\"", TokenType::string },

				{ ";[^\n]*", TokenType::comment },
				{ "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/", TokenType::comment },

				{ "[ \t]+", TokenType::whitespace },
				{ "\n", TokenType::newline }
			}
		};

		std::unordered_map<TokenType, std::string> tokenStr{
			{ TokenType::add, "add" },
			{ TokenType::addc, "addc" },
			{ TokenType::addi, "addi" },
			{ TokenType::sub, "sub" },
			{ TokenType::subc, "subc" },
			{ TokenType::subi, "subi" },
			{ TokenType::bxr, "bxr" },
			{ TokenType::bxri, "bxri" },
			{ TokenType::bor, "bor" },
			{ TokenType::bori, "bori" },
			{ TokenType::bnd, "bnd" },
			{ TokenType::bndi, "bndi" },
			{ TokenType::shiftl, "shiftl" },
			{ TokenType::shiftlc, "shiftlc" },
			{ TokenType::shiftli, "shiftli" },
			{ TokenType::shiftr, "shiftr" },
			{ TokenType::shiftrc, "shiftrc" },
			{ TokenType::shiftri, "shiftri" },
			{ TokenType::cmp, "cmp" },
			{ TokenType::cmpi, "cmpi" },
			{ TokenType::test, "test" },
			{ TokenType::testi, "testi" },
			{ TokenType::mov, "mov" },
			{ TokenType::set, "set" },
			{ TokenType::sets, "sets" },
			{ TokenType::push, "push" },
			{ TokenType::pop, "pop" },
			{ TokenType::ld, "ld" },
			{ TokenType::st, "st" },
			{ TokenType::jmp, "jmp" },
			{ TokenType::ijmp, "ijmp" },
			{ TokenType::call, "call" },
			{ TokenType::ret, "ret" },
			{ TokenType::nop, "nop" },
			{ TokenType::halt, "halt" },

			{ TokenType::reg, "reg" },
			{ TokenType::gen, "gen" },
			{ TokenType::sbp, "sbp" },
			{ TokenType::zero, "zero" },
			{ TokenType::one, "one" },
			{ TokenType::full, "full" },
			{ TokenType::pc, "pc" },
			{ TokenType::stack, "stack" },
			{ TokenType::flag, "flag" },

			{ TokenType::neg, "neg" },
			{ TokenType::pos, "pos" },
			{ TokenType::carry, "carry" },
			{ TokenType::carry4, "carry4" },
			{ TokenType::overflow, "overflow" },

			{ TokenType::_32B_, "32B" },
			{ TokenType::_16L_, "16L" },
			{ TokenType::_8L_, "8L" },
			{ TokenType::_8H_, "8H" },
			{ TokenType::_S16H_, "S16H" },
			{ TokenType::_S16L_, "S16L" },
			{ TokenType::_S8L_, "S8L" },
			{ TokenType::_S8H_, "S8H" },

			{ TokenType::text, ".text" },
			{ TokenType::data, ".data" },
			{ TokenType::bss, ".bss" },

			{ TokenType::define, "define" },
			{ TokenType::macro, "macro" },
			{ TokenType::end, "end" },

			{ TokenType::macro_arg, "macro_arg" },
			{ TokenType::identifier_unique, "identifier_unique" },

			{ TokenType::dot, "dot" },
			{ TokenType::comma, "comma" },
			{ TokenType::hash, "hash" },
			{ TokenType::openparen, "openparen" },
			{ TokenType::closeparen, "closeparen" },
			{ TokenType::openbrace, "openbrace" },
			{ TokenType::closebrace, "closebrace" },
			{ TokenType::openbrack, "openbrack" },
			{ TokenType::closebrack, "closebrack" },

			{ TokenType::colon, "colon" },
			{ TokenType::semicolon, "semicolon" },
			{ TokenType::plus, "plus" },
			{ TokenType::minus, "minus" },
			{ TokenType::dstar, "dstar" },
			{ TokenType::star, "star" },
			{ TokenType::slash, "slash" },
			{ TokenType::percent, "percent" },
			{ TokenType::tilde, "tilde" },
			{ TokenType::ampersend, "ampersend" },
			{ TokenType::verticalbar, "verticalbar" },
			{ TokenType::caret, "caret" },
			{ TokenType::leftshift, "leftshift" },
			{ TokenType::rightshift, "rightshift" },

			{ TokenType::hexnum, "hexnum" },
			{ TokenType::decnum, "decnum" },
			{ TokenType::octnum, "octnum" },
			{ TokenType::binnum, "binnum" },
			{ TokenType::comment, "comment" },
			{ TokenType::whitespace, "whitespace" },

			{ TokenType::newline, "newline" },
			{ TokenType::identifier, "identifier" },
			{ TokenType::string, "string" },

			{ TokenType::__epsilon, "epsilon" },
			{ TokenType::__unknown, "unknown" }
		};

		LexerFactory lexerFactory;
		Lexer lexer;

		inline void createLexer() {
			lexerFactory.setRules(CreateData);
			lexerFactory.update();
			lexer = lexerFactory.create();
		}

		inline void createLexer(const LexerFactory::UpdateData& data) {
			lexerFactory.directUpdate(data);
			lexer = lexerFactory.create();
		}

		inline void getFactoryData(LexerFactory::UpdateData& data) {
			lexerFactory.getData(data);
		}
	}

	namespace preprocesser {
		using u64 = unsigned __int64;

		using TokenType = lexer::TokenType;
		using Token = lexer::Token;

		using Preprocesser = preproccesser_generator::Preprocesser<Token>;
		using pPreprocFunc = Preprocesser::PreprocessFunction;
		using Vec = Preprocesser::Vec;
		using Iter = Preprocesser::Iter;

		namespace functions {
			int dec2int(std::string text) {
				bool neg = false;
				int ans = 0;

				auto it = text.begin();
				if (*it == '-') {
					neg = true;
					++it;
				}
				if (it != text.end() && *it == '0') ++it;
				if (it != text.end() && *it == 'd') ++it;

				for (it; it != text.end(); ++it) {
					ans *= 10;
					if ('0' <= *it && *it <= '9')
						ans += *it - '0';
				}
				if (neg)
					ans = -ans;
				return ans;
			}

			int macroArg2int(std::string text) {
				auto it = text.begin();
				if (it != text.end()) it++;

				int ans = 0;
				for (it; it != text.end(); ++it) {
					ans *= 10;
					if ('0' <= *it && *it <= '9')
						ans += *it - '0';
				}
				return ans;
			}

			enum class State {
				normal,
				readDefine_identifier,
				readDefine_token,

				readMacro_identifier,
				readMacro_argCount,
				readMacro_token,

				replace_getArg,
				replace_comma,
				replace
			};

			typedef struct _PreprocData {
				State state;

				std::string curReplaceString;
				std::vector<Token> curReplaceTokens;
				int curMacroArgCount;
				int depth;
				std::unordered_map<std::string, std::pair<std::vector<Token>, int>> replaceTable;

				std::vector<Token> curArg;
				std::vector<std::vector<Token>> args;
				int remainingArg;

				std::vector<Token> replaceTokens;

				int uniqueCounter;
			} PreprocData;

			bool handleReplace(Vec tokens, Iter it, void* vpdata) {
				PreprocData* data = (PreprocData*)vpdata;

				switch (data->state) {
				case State::replace_getArg:
					switch (it->type) {
					case TokenType::comma:
						if (data->depth == 1) {
							data->args.push_back(data->curArg);
							data->curArg.clear();
							data->remainingArg--;
						}
						else
							data->curArg.push_back(*it);
						it = tokens.erase(it);
						break;
					case TokenType::openparen:
						if (data->depth >= 1)
							data->curArg.push_back(*it);
						data->depth++;
						it = tokens.erase(it);
						break;
					case TokenType::closeparen:
						data->depth--; 
						if (!data->depth) {
							if (data->args.size()) {
								data->args.push_back(data->curArg);
								data->curArg.clear();
								data->remainingArg--;
							}
						}
						else
							data->curArg.push_back(*it);
						it = tokens.erase(it);
						break;
					default:
						data->curArg.push_back(*it);
						it = tokens.erase(it);
						break;
					}
					if (!data->remainingArg && !data->depth) {
						for (auto si = data->replaceTokens.begin(); si != data->replaceTokens.end(); ) {
							if (si->type == TokenType::macro_arg) {
								int id = macroArg2int(si->text) - 1;
								if (id < 0 || id >= data->args.size())
									return false;
								si = data->replaceTokens.erase(si);
								si = data->replaceTokens.insert(si, data->args[id].begin(), data->args[id].end());
							}
							else if (si->type == TokenType::identifier_unique) {
								Token tmp = {};
								tmp.text = si->text + "#" + std::to_string(data->uniqueCounter);
								tmp.type = TokenType::identifier;
								si = data->replaceTokens.erase(si);
								si = data->replaceTokens.insert(si, tmp);
							}
							else
								++si;
						}
						data->uniqueCounter++;
						it = tokens.insert(it, data->replaceTokens.begin(), data->replaceTokens.end());
						data->state = State::normal;
						return true;
					}
					break;

				case State::normal:
					switch (it->type) {
					case TokenType::define:
						data->state = State::readDefine_identifier;
						it = tokens.erase(it);
						break;

					case TokenType::macro:
						data->state = State::readMacro_identifier;
						it = tokens.erase(it);
						break;

					case TokenType::identifier: {
						auto find = data->replaceTable.find(it->text);
						if (find == data->replaceTable.end()) {
							++it;
							break;
						}

						it = tokens.erase(it);

						if (find->second.second == -1) {
							it = tokens.insert(it, find->second.first.begin(), find->second.first.end());
							break;
						}

						data->state = State::replace_getArg;
						data->replaceTokens = find->second.first;
						data->remainingArg = find->second.second;
						data->args.clear();
						data->depth = 0;

						break;
					}

					default:
						it++;
						break;
					}
					break;
				case State::readDefine_identifier:
					if (it->type == TokenType::identifier) {
						data->state = State::readDefine_token;
						data->curReplaceString = it->text;
						data->depth = 0;
						it = tokens.erase(it);
					}
					else
						return false;
					break;
				case State::readDefine_token:
					switch (it->type) {
					case TokenType::openparen:
						data->depth++;
						it = tokens.erase(it);
						break;
					case TokenType::closeparen:
						data->depth--;
						it = tokens.erase(it);
						break;
					default:
						data->curReplaceTokens.push_back(*it);
						it = tokens.erase(it);
						break;
					}
					if (!data->depth) {
						data->state = State::normal;
						data->replaceTable.insert({ data->curReplaceString, { data->curReplaceTokens, -1 } });
						data->curReplaceTokens.clear();
					}
					break;

				case State::readMacro_identifier:
					if (it->type == TokenType::identifier) {
						data->state = State::readMacro_argCount;
						data->curReplaceString = it->text;
						it = tokens.erase(it);
					}
					else
						return false;
					break;
				case State::readMacro_argCount:
					if (it->type == TokenType::decnum) {
						data->state = State::readMacro_token;
						data->curMacroArgCount = dec2int(it->text);
						it = tokens.erase(it);
					}
					else
						return false;
					break;
				case State::readMacro_token:
					switch (it->type) {
					case TokenType::end:
						data->state = State::normal;
						data->replaceTable.insert({ data->curReplaceString, { data->curReplaceTokens, data->curMacroArgCount } });
						data->curReplaceTokens.clear();
						it = tokens.erase(it);
						break;
					default:
						data->curReplaceTokens.push_back(*it);
						it = tokens.erase(it);
						break;
					}
					break;
				}

				return true;
			}

			bool removeUnused(Vec tokens, Iter it, void* vpdata) {
				PreprocData* data = (PreprocData*)vpdata;

				switch (it->type) {
				case TokenType::whitespace:
				case TokenType::newline:
				case TokenType::comment:
					it = tokens.erase(it);
					break;

				default:
					it++;
					break;
				}

				return true;
			}
		}

		Preprocesser preprocesser;

		inline void setTokens(std::vector<lexer::Token>& tokens) {
			preprocesser.setTokens(tokens);
		}

		inline bool preprocess(functions::PreprocData& data) {
			data.state = functions::State::normal;
			data.depth = 0;
			data.uniqueCounter = 0;
			if (!preprocesser.preprocess(functions::removeUnused, &data)) return false;
			if (!preprocesser.preprocess(functions::handleReplace, &data)) return false;

			return true;
		}
	}

	namespace parser {
		using u64 = unsigned __int64;

		using TokenType = lexer::TokenType;

		NTSTART(NonterminalType)
			program,
			section,

			text_section,
			data_section,
			bss_section,
			outer_section,

			intruction,
			label,
			label_text,
			label_data,
			label_bss,

			allocate,
			allocate_zero,

			reg,
			regid,
			regmode,
			reg_gen,
			reg_reg,

			flagid,

			index,

			immidate16,
			immidate8,

			expression,
			expressionL0,
			expressionL1,
			expressionL2,
			expressionL3,
			expressionL4,
			expressionL5,
			expressionL6,
			expressionL7,
			expressionL8,

			operatorL1,
			operatorL2,
			operatorL3,
			operatorL4,
			operatorL5,
			operatorL6,
			operatorL7,
			operatorL8,

			operationL1,
			operationL2,
			operationL3,
			operationL4,
			operationL5,
			operationL6,
			operationL7,
			operationL8,

			number,
			NTEND();

		enum class ASTNodeType {
			__epsilon = -1,
			__temp = 0,
			__accept = 1,

			program,

			text_section,
			data_section,
			bss_section,
			outer_section,

			allocate,
			allocate_zero,

			instruction_N,
			instruction_R,
			instruction_RR,
			instruction_RI,
			instruction_II,
			instruction_IRI,
			instruction_RVRI,
			instruction_FVRI,

			label,
			label_text,
			label_data,
			label_bss,

			reg,
			regid,
			regmode,
			reg_index,

			flagid,

			index,

			immidate16,
			immidate8,

			expression,
			operator_,
			operation,

			hex,
			dec,
			oct,
			bin
		};

		std::unordered_map<NonterminalType, std::string> nonterminalStr{
			{ NonterminalType::__accept, "accept" },

			{ NonterminalType::program, "program" },
			{ NonterminalType::section, "section" },

			{ NonterminalType::text_section, "text_section" },
			{ NonterminalType::data_section, "data_section" },
			{ NonterminalType::bss_section, "bss_section" },
			{ NonterminalType::outer_section, "outer_section" },

			{ NonterminalType::intruction, "intruction" },
			{ NonterminalType::label, "label" },
			{ NonterminalType::label_text, "label_text" },
			{ NonterminalType::label_data, "label_data" },
			{ NonterminalType::label_bss, "label_bss" },
			{ NonterminalType::allocate, "allocate" },
			{ NonterminalType::allocate_zero, "allocate_zero" },

			{ NonterminalType::reg, "reg" },
			{ NonterminalType::regid, "regid" },
			{ NonterminalType::regmode, "regmode" },
			{ NonterminalType::reg_gen, "reg_gen" },
			{ NonterminalType::reg_reg, "reg_reg" },

			{ NonterminalType::flagid, "flagid" },

			{ NonterminalType::index, "index" },

			{ NonterminalType::immidate16, "immidate16" },
			{ NonterminalType::immidate8, "immidate8" },

			{ NonterminalType::expression, "expression" },

			{ NonterminalType::expressionL0, "expressionL0" },
			{ NonterminalType::expressionL1, "expressionL1" },
			{ NonterminalType::expressionL2, "expressionL2" },
			{ NonterminalType::expressionL3, "expressionL3" },
			{ NonterminalType::expressionL4, "expressionL4" },
			{ NonterminalType::expressionL5, "expressionL5" },
			{ NonterminalType::expressionL6, "expressionL6" },
			{ NonterminalType::expressionL7, "expressionL7" },
			{ NonterminalType::expressionL8, "expressionL8" },

			{ NonterminalType::operatorL1, "operatorL1" },
			{ NonterminalType::operatorL2, "operatorL2" },
			{ NonterminalType::operatorL3, "operatorL3" },
			{ NonterminalType::operatorL4, "operatorL4" },
			{ NonterminalType::operatorL5, "operatorL5" },
			{ NonterminalType::operatorL6, "operatorL6" },
			{ NonterminalType::operatorL7, "operatorL7" },
			{ NonterminalType::operatorL8, "operatorL8" },

			{ NonterminalType::operationL1, "operationL1" },
			{ NonterminalType::operationL2, "operationL2" },
			{ NonterminalType::operationL3, "operationL3" },
			{ NonterminalType::operationL4, "operationL4" },
			{ NonterminalType::operationL5, "operationL5" },
			{ NonterminalType::operationL6, "operationL6" },
			{ NonterminalType::operationL7, "operationL7" },
			{ NonterminalType::operationL8, "operationL8" },

			{ NonterminalType::number, "number" }
		};

		std::unordered_map<ASTNodeType, std::string> asttypeStr = {
			{ ASTNodeType::__epsilon, "epsilon" },
			{ ASTNodeType::__temp, "temp" },
			{ ASTNodeType::__accept, "accept" },

			{ ASTNodeType::program, "program" },

			{ ASTNodeType::text_section, "text_section" },
			{ ASTNodeType::data_section, "data_section" },
			{ ASTNodeType::bss_section, "bss_section" },
			{ ASTNodeType::outer_section, "outer_section" },

			{ ASTNodeType::instruction_N, "instruction_N" },
			{ ASTNodeType::instruction_R, "instruction_R" },
			{ ASTNodeType::instruction_RR, "instruction_RR" },
			{ ASTNodeType::instruction_RI, "instruction_RI" },
			{ ASTNodeType::instruction_II, "instruction_II" },
			{ ASTNodeType::instruction_IRI, "instruction_IRI" },
			{ ASTNodeType::instruction_RVRI, "instruction_RVRI" },
			{ ASTNodeType::instruction_FVRI, "instruction_FVRI" },

			{ ASTNodeType::allocate, "allocate" },
			{ ASTNodeType::allocate_zero, "allocate_zero" },
			{ ASTNodeType::label, "label" },
			{ ASTNodeType::label_text, "label_text" },
			{ ASTNodeType::label_data, "label_data" },
			{ ASTNodeType::label_bss, "label_bss" },

			{ ASTNodeType::reg, "reg" },
			{ ASTNodeType::regid, "regid" },
			{ ASTNodeType::regmode, "regmode" },
			{ ASTNodeType::reg_index, "reg_index" },

			{ ASTNodeType::flagid, "flagid" },

			{ ASTNodeType::index, "index" },

			{ ASTNodeType::immidate16, "immidate16" },
			{ ASTNodeType::immidate8, "immidate8" },

			{ ASTNodeType::expression, "expression" },
			{ ASTNodeType::operator_, "operator" },
			{ ASTNodeType::operation, "operation" },

			{ ASTNodeType::hex, "hex" },
			{ ASTNodeType::dec, "dec" },
			{ ASTNodeType::oct, "oct" },
			{ ASTNodeType::bin, "bin" }
		};

		using ParserFactory = parser_generator::ParserFactory<TokenType, NonterminalType, ASTNodeType>;
		using PFCD = ParserFactory::CreateData;
		using Parser = parser_generator::Parser<TokenType, NonterminalType, ASTNodeType>;
		using Tree = parser_generator::PTNode;
		using NT = NonterminalType;
		using TT = TokenType;
		using AT = ASTNodeType;

		const PFCD CreateData = {
			{ { NT::__accept, AT::__accept, -1 }, { { NT::program, true, false } } },

			{ { NT::program, AT::program, -1}, { { NT::section, true, true } } },

			{ { NT::section, AT::text_section, -1 }, { { TT::text, false , false }, { NT::text_section, true, false }, { NT::section, true, true } } },
			{ { NT::section, AT::data_section, -1 }, { { TT::data, false, false }, { NT::data_section, true, false }, { NT::section, true, true } } },
			{ { NT::section, AT::bss_section, -1 }, { { TT::bss, false, false }, { NT::bss_section, true, false }, { NT::section, true, true } } },
			{ { NT::section, AT::outer_section, -1 }, { { NT::outer_section, true, false }, { NT::section, true, true } } },

			{ { NT::section, AT::text_section, -1 }, { { TT::text, false, false }, { NT::text_section, true, false } } },
			{ { NT::section, AT::data_section, -1 }, { { TT::data, false, false }, { NT::data_section, true, false} } },
			{ { NT::section, AT::bss_section, -1 }, { { TT::bss, false, false }, { NT::bss_section, true, false } } },
			{ { NT::section, AT::outer_section, -1 }, { { NT::outer_section, true, false } } },

			{ { NT::section, AT::text_section, -1 }, { { TT::text, false , false }, { NT::section, true, true } } },
			{ { NT::section, AT::data_section, -1 }, { { TT::data, false, false }, { NT::section, true, true } } },
			{ { NT::section, AT::bss_section, -1 }, { { TT::bss, false, false }, { NT::section, true, true } } },

			{ { NT::section, AT::text_section, -1 }, { { TT::text, false, false } } },
			{ { NT::section, AT::data_section, -1 }, { { TT::data, false, false } } },
			{ { NT::section, AT::bss_section, -1 }, { { TT::bss, false, false } } },

			{ { NT::data_section, AT::data_section, -1 }, { { NT::allocate, true, false }, { NT::data_section, true, true } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::label_data, true, false }, { NT::data_section, true, true } } },

			{ { NT::data_section, AT::data_section, -1 }, { { NT::allocate, true, false } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::label_data, true, false } } },

			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::allocate_zero, true, false }, { NT::bss_section, true, true } } },
			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::label_bss, true, false }, { NT::bss_section, true, true } } },

			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::allocate_zero, true, false } } },
			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::label_bss, true, false } } },

			{ { NT::text_section, AT::text_section, -1 }, { { NT::intruction, true, false }, { NT::text_section, true, true } } },
			{ { NT::text_section, AT::text_section, -1 }, { { NT::label_text, true, false }, { NT::text_section, true, true } } },

			{ { NT::text_section, AT::text_section, -1 }, { { NT::intruction, true, false } } },
			{ { NT::text_section, AT::text_section, -1 }, { { NT::label_text, true, false } } },

			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::add, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::addc, false, false }, {NT::reg, true, false }, { TT::comma, false, false } ,{ NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::addi, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::sub, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::subc, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::subi, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::bxr, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::bxri, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::bor, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::bori, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::bnd, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::bndi, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::shiftl, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::shiftlc, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::shiftli, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::shiftr, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::shiftrc, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::shiftri, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::ror, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::rori, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::rol, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::roli, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::cmp, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::cmpi, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::test, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::testi, false, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },

			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::set, false, false }, { NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::sets, false, false }, { NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, 0 }, { { TT::mov, false, false }, { NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },

			{ { NT::intruction, AT::instruction_R, 0 }, { { TT::push, false, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_R, 0 }, { { TT::pop, false, false }, {NT::reg, true, false } } },

			{ { NT::intruction, AT::instruction_RVRI, 0 }, { { TT::ld, false, false }, { TT::dot, false, false }, { NT::regid, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RVRI, 0 }, { { TT::st, false, false }, { TT::dot, false, false }, { NT::regid, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },

			{ { NT::intruction, AT::instruction_FVRI, 0 }, { { TT::jmp, false, false }, { TT::dot, false, false }, { NT::flagid, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_FVRI, 0 }, { { TT::ijmp, false, false }, { TT::dot, false, false }, { NT::flagid, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RI, 0 }, { { TT::call, false, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_N, 0 }, { { TT::ret, false, false } } },
			{ { NT::intruction, AT::instruction_N, 0 }, { { TT::nop, false, false } } },
			{ { NT::intruction, AT::instruction_N, 0 }, { { TT::halt, false, false } } },

			{ { NT::reg, AT::reg, 0 }, { { NT::regid, true, false }, { TT::dot, false, false }, { NT::regmode, true, false } } },

			{ { NT::regid, AT::__epsilon, 0 }, { { NT::reg_gen, true, true } } },
			{ { NT::regid, AT::__epsilon, 0 }, { { NT::reg_reg, true, true } } },
			{ { NT::regid, AT::regid, 0 }, { { TT::sbp, false, false } } },
			{ { NT::regid, AT::regid, 0 }, { { TT::zero, false, false } } },
			{ { NT::regid, AT::regid, 0 }, { { TT::one, false, false } } },
			{ { NT::regid, AT::regid, 0 }, { { TT::full, false, false } } },
			{ { NT::regid, AT::regid, 0 }, { { TT::pc, false, false } } },
			{ { NT::regid, AT::regid, 0 }, { { TT::stack, false, false } } },
			{ { NT::regid, AT::regid, 0 }, { { TT::flag, false, false } } },

			{ { NT::reg_gen, AT::reg_index, 0 }, { { TT::gen, false, false }, { NT::index, true, false } } },
			{ { NT::reg_reg, AT::reg_index, 0 }, { { TT::reg, false, false }, { NT::index, true, false } } },

			{ { NT::regmode, AT::regmode, 0 }, { { TT::_32B_, false, false } } },
			{ { NT::regmode, AT::regmode, 0 }, { { TT::_16L_, false, false } } },
			{ { NT::regmode, AT::regmode, 0 }, { { TT::_S16H_, false, false } } },
			{ { NT::regmode, AT::regmode, 0 }, { { TT::_8L_, false, false } } },
			{ { NT::regmode, AT::regmode, 0 }, { { TT::_8H_, false, false } } },
			{ { NT::regmode, AT::regmode, 0 }, { { TT::_S16L_, false, false } } },
			{ { NT::regmode, AT::regmode, 0 }, { { TT::_S8L_, false, false } } },
			{ { NT::regmode, AT::regmode, 0 }, { { TT::_S8H_, false, false } } },

			{ { NT::flagid, AT::flagid, 0 }, { { TT::zero, false, false } } },
			{ { NT::flagid, AT::flagid, 0 }, { { TT::neg, false, false } } },
			{ { NT::flagid, AT::flagid, 0 }, { { TT::pos, false, false } } },
			{ { NT::flagid, AT::flagid, 0 }, { { TT::carry, false, false } } },
			{ { NT::flagid, AT::flagid, 0 }, { { TT::carry4, false, false } } },
			{ { NT::flagid, AT::flagid, 0 }, { { TT::overflow, false, false } } },
			{ { NT::flagid, AT::flagid, 0 }, { { TT::one, false, false } } },
			{ { NT::flagid, AT::flagid, 0 }, { { TT::gen, false, false } } },

			{ { NT::immidate16, AT::immidate16, -1 }, { { NT::expression, true, false } } },
			{ { NT::immidate8, AT::immidate8, -1 }, { { NT::expression, true, false } } },

			{ { NT::label_text, AT::label_text, 0 }, { { NT::label, true, true } } },
			{ { NT::label_data, AT::label_data, 0 }, { { NT::label, true, true } } },
			{ { NT::label_bss, AT::label_bss, 0 }, { { NT::label, true, true } } },
			{ { NT::label, AT::label, 0 }, { { TT::identifier, false, false }, { TT::colon, false, false } } },

			{ { NT::allocate, AT::allocate, 0 }, { { NT::expression, true, false }, { TT::comma, false, false },  { NT::expression, true, false } } },
			{ { NT::allocate_zero, AT::allocate_zero, 0 }, { { NT::expression, true, false } } },

			{ { NT::index, AT::index, -1 }, { { TT::openbrack, false, false }, { NT::expression, true, false }, { TT::closebrack, false, false } } },

			{ { NT::expression, AT::__epsilon, 0 }, { { NT::expressionL8, true, true } } },

			{ { NT::expressionL8, AT::__epsilon, 1 }, { { NT::expressionL8, true, false }, { TT::verticalbar, false, false }, { NT::expressionL7, true, false } } },
			{ { NT::expressionL8, AT::__epsilon, 0 }, { { NT::expressionL7, true, true } } },

			{ { NT::expressionL7, AT::__epsilon, 1 }, { { NT::expressionL7, true, false }, { TT::caret, false, false }, { NT::expressionL6, true, false } } },
			{ { NT::expressionL7, AT::__epsilon, 0 }, { { NT::expressionL6, true, true } } },

			{ { NT::expressionL6, AT::__epsilon, 1 }, { { NT::expressionL6, true, false }, { TT::ampersend, false, false }, { NT::expressionL5, true, false } } },
			{ { NT::expressionL6, AT::__epsilon, 0 }, { { NT::expressionL5, true, true } } },

			{ { NT::expressionL5, AT::__epsilon, 1 }, { { NT::expressionL5, true, false }, { TT::leftshift, false, false }, { NT::expressionL4, true, false } } },
			{ { NT::expressionL5, AT::__epsilon, 1 }, { { NT::expressionL5, true, false }, { TT::rightshift, false, false }, { NT::expressionL4, true, false } } },
			{ { NT::expressionL5, AT::__epsilon, 0 }, { { NT::expressionL4, true, true } } },

			{ { NT::expressionL4, AT::__epsilon, 1 }, { { NT::expressionL4, true, false }, { TT::plus, false, false }, { NT::expressionL3, true, false } } },
			{ { NT::expressionL4, AT::__epsilon, 1 }, { { NT::expressionL4, true, false }, { TT::minus, false, false }, { NT::expressionL3, true, false } } },
			{ { NT::expressionL4, AT::__epsilon, 0 }, { { NT::expressionL3, true, true } } },

			{ { NT::expressionL3, AT::__epsilon, 1 }, { { NT::expressionL3, true, false }, { TT::star, false, false }, { NT::expressionL2, true, false } } },
			{ { NT::expressionL3, AT::__epsilon, 1 }, { { NT::expressionL3, true, false }, { TT::slash, false, false }, { NT::expressionL2, true, false } } },
			{ { NT::expressionL3, AT::__epsilon, 1 }, { { NT::expressionL3, true, false }, { TT::percent, false, false }, { NT::expressionL2, true, false} } },
			{ { NT::expressionL3, AT::__epsilon, 0 }, { { NT::expressionL2, true, true } } },

			{ { NT::expressionL2, AT::__epsilon, 1 }, { { NT::expressionL1, true, false }, { TT::dstar, false, false }, { NT::expressionL2, true, false } } },
			{ { NT::expressionL2, AT::__epsilon, 0 }, { { NT::expressionL1, true, true } } },

			{ { NT::expressionL1, AT::__epsilon, 0 }, { { TT::tilde, false, false }, { NT::expressionL0, true, false } } },
			{ { NT::expressionL1, AT::__epsilon, 0 }, { { NT::expressionL0, true, true } } },

			{ { NT::expressionL0, AT::__epsilon, 1 }, { { TT::openparen, false, false }, { NT::expressionL7, true, true }, { TT::closeparen, false, false } } },
			{ { NT::expressionL0, AT::__epsilon, 0 }, { { NT::number, false , false } } },
			{ { NT::expressionL0, AT::__epsilon, 0 }, { { TT::identifier, false , false } } },

			{ { NT::number, AT::__epsilon, 0 }, { { TT::hexnum, false, false } } },
			{ { NT::number, AT::__epsilon, 0 }, { { TT::decnum, false, false } } },
			{ { NT::number, AT::__epsilon, 0 }, { { TT::octnum, false, false } } },
			{ { NT::number, AT::__epsilon, 0 }, { { TT::binnum, false, false } } }
		};

		ParserFactory parserFactory;
		Parser parser;

		inline void createParser(void) {
			parserFactory.setRules(CreateData);

			parserFactory.update();
			parser = parserFactory.create();
		}

		inline void createParser(ParserFactory::UpdateData& data) {
			parserFactory.directUpdate(data);
			parser = parserFactory.create();
		}

		inline void getFactoryData(ParserFactory::UpdateData& data) {
			parserFactory.getData(data);
		}
	}

	namespace evaluator {
		using u64 = unsigned __int64;
		using u32 = unsigned __int32;
		using s32 = __int32;
		using u8 = unsigned __int8;

		using TokenType = lexer::TokenType;
		using NonterminalType = parser::NonterminalType;
		using ASTNodeType = parser::ASTNodeType;
		using AST = parser::Parser::ASTNode;

		using Evaluator = evaluator_generator::Evaluator<TokenType, NonterminalType, ASTNodeType>;
		using pEvalFunc = Evaluator::EvaluateFunction;

		namespace functions {
			typedef struct _EvalData {
				u32	textByte;
				u32 dataByte;
				u64 bssByte;

				std::unordered_map<std::string, u32> idenifierValue;
				std::unordered_map<AST*, s32> expressionValue;

				std::unordered_map<AST*, u32> instructionValue;
				std::unordered_map<AST*, std::pair<u32, u32>> dataValue;

				std::vector<u8> textSection;
				std::vector<u8> dataSection;
				std::vector<u8> bssSection;

				std::vector<u8> rawData;

				_EvalData() : textByte(0), dataByte(0), bssByte(0) {

				}
			} EvalData;

			int hex2int(std::string text) {
				bool neg = false;
				int ans = 0;

				auto it = text.begin();
				if (*it == '-') {
					neg = true;
					++it;
				}
				if (it != text.end()) ++it;
				if (it != text.end()) ++it;

				for (it; it != text.end(); ++it) {
					ans *= 16;
					if ('0' <= *it && *it <= '9')
						ans += *it - '0';
					else if ('a' <= *it && *it <= 'f')
						ans += *it - 'a' + 10;
					else
						ans += *it - 'A' + 10;
				}
				if (neg)
					ans = -ans;
				return ans;
			}

			int dec2int(std::string text) {
				bool neg = false;
				int ans = 0;

				auto it = text.begin();
				if (*it == '-') {
					neg = true;
					++it;
				}
				if (it != text.end() && *it == '0') ++it;
				if (it != text.end() && *it == 'd') ++it;

				for (it; it != text.end(); ++it) {
					ans *= 10;
					if ('0' <= *it && *it <= '9')
						ans += *it - '0';
				}
				if (neg)
					ans = -ans;
				return ans;
			}

			int oct2int(std::string text) {
				bool neg = false;
				int ans = 0;

				auto it = text.begin();
				if (*it == '-') {
					neg = true;
					++it;
				}
				if (it != text.end()) ++it;
				if (it != text.end()) ++it;

				for (it; it != text.end(); ++it) {
					ans *= 8;
					if ('0' <= *it && *it <= '7')
						ans += *it - '0';
				}
				if (neg)
					ans = -ans;
				return ans;
			}

			int bin2int(std::string text) {
				bool neg = false;
				int ans = 0;

				auto it = text.begin();
				if (*it == '-') {
					neg = true;
					++it;
				}
				if (it != text.end()) ++it;
				if (it != text.end()) ++it;

				for (it; it != text.end(); ++it) {
					ans *= 2;
					if ('0' <= *it && *it <= '1')
						ans += *it - '0';
				}
				if (neg)
					ans = -ans;
				return ans;
			}

			int pow(int base, u32 power) {
				int ans = 1;
				for (u32 i = 0; i < power; i++)
					ans *= base;
				return ans;
			}

			std::map<std::string, u32> instructionBase = {	
				{ "add", 0x0 },
				{ "addc", 0x1 },
				{ "addi", 0x2 },
				{ "sub", 0x3 },
				{ "subc", 0x4 },
				{ "subi", 0x5 },
				{ "bxr", 0x6 },
				{ "bxri", 0x7 },
				{ "bor", 0x8 },
				{ "bori", 0x9 },
				{ "bnd", 0xA },
				{ "bndi", 0xB },
				{ "rol", 0xC },
				{ "roli", 0xD },
				{ "ror", 0xE },
				{ "rori", 0xF },
				{ "shiftl", 0x10 },
				{ "shiftlc", 0x11 },
				{ "shiftli", 0x12 },
				{ "shiftr", 0x13 },
				{ "shiftrc", 0x14 },
				{ "shiftri", 0x15 },
				{ "cmp", 0x16 },
				{ "cmpi", 0x17 },
				{ "test", 0x18 },
				{ "testi", 0x19 },
				{ "set", 0x1A },
				{ "sets", 0x1B },
				{ "mov", 0x1C },
				{ "pop", 0x1E },
				{ "push", 0x1F },
				{ "ld", 0x20 },
				{ "st", 0x40 },
				{ "jmp", 0x60 },
				{ "ijmp", 0x68 },
				{ "call", 0x70 },
				{ "ret", 0x71 },
				{ "nop", 0x72 },
				{ "halt", 0x73 }
			};

			std::map<std::string, u32> regBase = {
				{ "reg", 0x0 },
				{ "gen", 0x0 },
				{ "sbp", 0x19 },
				{ "zero", 0x1A },
				{ "one", 0x1B },
				{ "full", 0x1C },
				{ "pc", 0x1D },
				{ "stack", 0x1E },
				{ "flag", 0x1F },

				{ "32B", 0 }, { "32b", 0 },
				{ "16L", 1 }, { "16l", 1 },
				{ "8L", 2 }, { "8l", 2 },
				{ "8H", 3 }, { "8h", 3 },
				{ "S16H", 4 }, { "S16h", 4 }, { "s16H", 4 }, { "s16h", 4 }, 
				{ "S16L", 5 }, { "S16l", 5 }, { "s16L", 5 }, { "s16l", 5 },
				{ "S8L", 6 }, { "S8l", 6 }, { "s8L", 6 }, { "s8l", 6 },
				{ "S8H", 7 }, { "S8h", 7 }, { "s8H", 7 }, { "s8h", 7 }
			};

			std::map<std::string, u32> flagBase = {
				{ "zero", 0 },
				{ "neg", 1 },
				{ "pos", 2 },
				{ "carry", 3 },
				{ "carry4", 4 },
				{ "overflow", 5 },
				{ "one", 6 },
				{ "gen", 7 },
			};

			bool calculateConst(AST* cur, void* vpData) {
				EvalData* data = (EvalData*)vpData;

				bool child1exp = (cur->child.size() >= 1) ? data->expressionValue.find(cur->child[0]) != data->expressionValue.end() : false;
				bool child2exp = (cur->child.size() >= 2) ? data->expressionValue.find(cur->child[1]) != data->expressionValue.end() : false;

				switch (cur->type) {
				case (u64)TokenType::verticalbar: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] | data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::caret: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] ^ data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::ampersend: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] & data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::leftshift: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] << data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::rightshift: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] >> data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::plus: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] + data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::minus: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] - data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::star: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] * data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::slash: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] / data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::percent: if (child1exp && child2exp) { data->expressionValue[cur] = data->expressionValue[cur->child[0]] % data->expressionValue[cur->child[1]]; } break;
				case (u64)TokenType::dstar: if (child1exp && child2exp) { data->expressionValue[cur] = pow(data->expressionValue[cur->child[0]], data->expressionValue[cur->child[1]]); } break;
				case (u64)TokenType::tilde: if (child1exp) { data->expressionValue[cur] = ~data->expressionValue[cur->child[0]]; } break;

				case (u64)TokenType::hexnum: data->expressionValue[cur] = hex2int(cur->text); break;
				case (u64)TokenType::decnum: data->expressionValue[cur] = dec2int(cur->text); break;
				case (u64)TokenType::octnum: data->expressionValue[cur] = oct2int(cur->text); break;
				case (u64)TokenType::binnum: data->expressionValue[cur] = bin2int(cur->text); break;
				}

				return true;
			}

			bool calculateSize(AST* cur, void* vpData) {
				EvalData* data = (EvalData*)vpData;

				switch (cur->type) {
				case (u64)ASTNodeType::instruction_N + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_R + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RR + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_II + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_IRI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RVRI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_FVRI + (u64)TokenType::__end:
					data->textByte += 4;
					break;

				case (u64)ASTNodeType::allocate + (u64)TokenType::__end:
					if (data->expressionValue.find(cur->child[0]) == data->expressionValue.end())
						return false;
					data->dataByte += data->expressionValue[cur->child[0]];
					break;
				}

				return true;
			}

			bool calculateLabel(AST* cur, void* vpData) {
				EvalData* data = (EvalData*)vpData;

				switch (cur->type) {

				case (u64)ASTNodeType::instruction_N + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_R + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RR + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_II + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_IRI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RVRI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_FVRI + (u64)TokenType::__end:
					data->textByte += 4;
					break;

				case (u64)ASTNodeType::label_text + (u64)TokenType::__end:
					data->idenifierValue[cur->text] = data->textByte;
					break;

				case (u64)ASTNodeType::allocate + (u64)TokenType::__end:
					if (data->expressionValue.find(cur->child[0]) == data->expressionValue.end())
						return false;
					data->dataByte += data->expressionValue[cur->child[0]];
					data->dataValue[cur].second = data->expressionValue[cur->child[0]];
					break;

				case (u64)ASTNodeType::label_data + (u64)TokenType::__end:
					data->idenifierValue[cur->text] = data->dataByte;
					break;
				}

				return true;
			}

			bool calculateNonconst(AST* cur, void* vpData) {
				EvalData* data = (EvalData*)vpData;

				switch (cur->type) {
				case (u64)ASTNodeType::index + (u64)TokenType::__end:
					if ((s32)data->expressionValue[cur->child[0]] < 0)
						return false;
					data->expressionValue[cur] = data->expressionValue[cur->child[0]];
					break;

				case (u64)TokenType::verticalbar: data->expressionValue[cur] = data->expressionValue[cur->child[0]] | data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::caret: data->expressionValue[cur] = data->expressionValue[cur->child[0]] ^ data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::ampersend: data->expressionValue[cur] = data->expressionValue[cur->child[0]] & data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::leftshift: data->expressionValue[cur] = data->expressionValue[cur->child[0]] << data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::rightshift: data->expressionValue[cur] = data->expressionValue[cur->child[0]] >> data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::plus: data->expressionValue[cur] = data->expressionValue[cur->child[0]] + data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::minus: data->expressionValue[cur] = data->expressionValue[cur->child[0]] - data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::star: data->expressionValue[cur] = data->expressionValue[cur->child[0]] * data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::slash: data->expressionValue[cur] = data->expressionValue[cur->child[0]] / data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::percent: data->expressionValue[cur] = data->expressionValue[cur->child[0]] % data->expressionValue[cur->child[1]]; break;
				case (u64)TokenType::dstar: data->expressionValue[cur] = pow(data->expressionValue[cur->child[0]], data->expressionValue[cur->child[1]]); break;
				case (u64)TokenType::tilde: data->expressionValue[cur] = ~data->expressionValue[cur->child[0]]; break;

				case (u64)TokenType::identifier:
					if (data->idenifierValue.find(cur->text) == data->idenifierValue.end())
						return false;
					data->expressionValue[cur] = data->idenifierValue[cur->text];
					break;
				}

				return true;
			}

			void push32(std::vector<u8>& list, u32 inst) {
				union {
					u32 u32data;
					u8 u8data[4];
				} conv;

				conv.u32data = inst;
				list.push_back(conv.u8data[3]);
				list.push_back(conv.u8data[2]);
				list.push_back(conv.u8data[1]);
				list.push_back(conv.u8data[0]);
			}

			void pushByteN(std::vector<u8>& list, u32 data, int size) {
				union {
					u32 u32data;
					u8 u8data[4];
				} conv;
				conv.u32data = data;

				for (size; size > 4; size--) {
					list.push_back(conv.u8data[(size-1)%4]);
				}

				switch (size) {
				case 4:
					list.push_back(conv.u8data[3]);
				case 3:
					list.push_back(conv.u8data[2]);
				case 2:
					list.push_back(conv.u8data[1]);
				case 1:
					list.push_back(conv.u8data[0]);
				}
			}

			bool evaluateCode(AST* cur, void* vpData) {
				EvalData* data = (EvalData*)vpData;
				switch (cur->type) {

				case (u64)ASTNodeType::allocate + (u64)TokenType::__end:
					if (data->expressionValue.find(cur->child[1]) == data->expressionValue.end())
						return false;
					data->dataValue[cur].first = data->expressionValue[cur->child[1]];
					pushByteN(data->dataSection, data->dataValue[cur].first, data->dataValue[cur].second);
					break;

				case (u64)ASTNodeType::instruction_N + (u64)TokenType::__end:
					data->instructionValue[cur] = (instructionBase[cur->text] << 24);
					push32(data->textSection, data->instructionValue[cur]);
					break;
				case (u64)ASTNodeType::instruction_R + (u64)TokenType::__end:
					data->instructionValue[cur] = (instructionBase[cur->text] << 24) + (data->instructionValue[cur->child[0]] << 16);
					push32(data->textSection, data->instructionValue[cur]);
					break;
				case (u64)ASTNodeType::instruction_RI + (u64)TokenType::__end:
					data->instructionValue[cur] = (instructionBase[cur->text] << 24) + (data->instructionValue[cur->child[0]] << 16) + (data->instructionValue[cur->child[1]]);
					push32(data->textSection, data->instructionValue[cur]);
					break;
				case (u64)ASTNodeType::instruction_RR + (u64)TokenType::__end:
					data->instructionValue[cur] = (instructionBase[cur->text] << 24) + (data->instructionValue[cur->child[0]] << 16) + (data->instructionValue[cur->child[1]] << 8);
					push32(data->textSection, data->instructionValue[cur]);
					break;
				case (u64)ASTNodeType::instruction_II + (u64)TokenType::__end:
					data->instructionValue[cur] = (instructionBase[cur->text] << 24) + (data->instructionValue[cur->child[0]] << 16) + (data->instructionValue[cur->child[1]]);
					push32(data->textSection, data->instructionValue[cur]);
					break;
				case (u64)ASTNodeType::instruction_IRI + (u64)TokenType::__end:
					data->instructionValue[cur] = (instructionBase[cur->text] << 24) + (data->instructionValue[cur->child[0]] << 16) + (data->instructionValue[cur->child[1]] << 8) + (data->instructionValue[cur->child[2]]);
					push32(data->textSection, data->instructionValue[cur]);
					break;
				case (u64)ASTNodeType::instruction_RVRI + (u64)TokenType::__end:
					data->instructionValue[cur] = ((instructionBase[cur->text] + data->instructionValue[cur->child[0]]) << 24) + (data->instructionValue[cur->child[1]] << 16) + (data->instructionValue[cur->child[2]]);
					push32(data->textSection, data->instructionValue[cur]);
					break;
				case (u64)ASTNodeType::instruction_FVRI + (u64)TokenType::__end:
					data->instructionValue[cur] = ((instructionBase[cur->text] + flagBase[cur->child[0]->text]) << 24) + (data->instructionValue[cur->child[1]] << 16) + (data->instructionValue[cur->child[2]]);
					push32(data->textSection, data->instructionValue[cur]);
					break;

				case (u64)ASTNodeType::reg + (u64)TokenType::__end:
					data->instructionValue[cur] = (data->instructionValue[cur->child[0]] << 3) + data->instructionValue[cur->child[1]];
					break;
				case (u64)ASTNodeType::regid + (u64)TokenType::__end:
					data->instructionValue[cur] = regBase[cur->text];
					break;
				case (u64)ASTNodeType::reg_index + (u64)TokenType::__end:
					data->instructionValue[cur] = regBase[cur->text] + data->expressionValue[cur->child[0]];
					break;
				case (u64)ASTNodeType::regmode + (u64)TokenType::__end:
					data->instructionValue[cur] = regBase[cur->text];
					break;

				case (u64)ASTNodeType::immidate16 + (u64)TokenType::__end:
					data->instructionValue[cur] = data->expressionValue[cur->child[0]] & 0xFFFF;
					break;
				case (u64)ASTNodeType::immidate8 + (u64)TokenType::__end:
					data->instructionValue[cur] = data->expressionValue[cur->child[0]] & 0xFF;
					break;
				}

				return true;
			}
		}

		Evaluator evaluator;

		inline void setTree(AST* tree) {
			evaluator.setTree(tree);
		}

		inline bool evaluate(functions::EvalData& data) {
			data.textByte = 0;
			data.dataByte = 0;
			data.bssByte = 0;

			if (!evaluator.evaluate(functions::calculateConst, &data)) return false;
			if (!evaluator.evaluate(functions::calculateSize, &data)) return false;

			data.bssByte = data.dataByte + data.textByte - 1;
			data.dataByte = data.textByte;
			data.textByte = 0;

			if (!evaluator.evaluate(functions::calculateLabel, &data)) return false;
			if (!evaluator.evaluate(functions::calculateNonconst, &data)) return false;
			if (!evaluator.evaluate(functions::evaluateCode, &data)) return false;

			data.rawData.insert(data.rawData.end(), data.textSection.begin(), data.textSection.end());
			data.rawData.insert(data.rawData.end(), data.dataSection.begin(), data.dataSection.end());

			return true;
		}
	}

	const unsigned long long version = 202;

	namespace cache {
		using u64 = unsigned __int64;
		using u8 = unsigned __int8;

		using LexerData = lexer::LexerFactory::UpdateData;
		using ParserData = parser::ParserFactory::UpdateData;

		inline void pack(std::vector<u64>& dest, const std::vector<u8>& src) {
			int count = 0;
			u64 tmp = 0;
			for (auto it = src.cbegin(); it != src.cend(); ++it) {
				tmp = (tmp << 8) + *it;

				count++; count %= 8;
				if (!count) {
					dest.push_back(tmp);
					tmp = 0;
				}
			}
			if (count) {
				tmp = (tmp << (8 * (8 - count)));
				dest.push_back(tmp);
			}
		}

		inline void unpack(std::vector<u8>& dest, const std::vector<u64>& src) {
			for (auto it = src.cbegin(); it != src.cend(); ++it) {
				u64 tmp = *it;
				for (int i = 0; i < 8; i++) {
					dest.push_back((u8)(tmp >> 56));
					tmp <<= 8;
				}
			}
		}

		void compress(std::vector<u8>& out, const std::vector<u8>& in) {
			u8 byte = in.front();
			int count = -1;
			for (auto it = in.cbegin(); it != in.cend(); ++it) {
				if (byte != *it || count >= 255) {
					out.push_back(byte);
					out.push_back((u8)count);
					byte = *it;
					count = 0;
					continue;
				}
				count++;
			}
			out.push_back(byte);
			out.push_back((u8)count);
		}

		void decompress(std::vector<u8>& out, const std::vector<u8>& in) {
			for (auto it = in.cbegin(); it != in.cend(); ) {
				u8 byte = *it; ++it;
				u8 count = *it; ++it;
				for (int i = 0; i <= (int)count; i++)
					out.push_back(byte);
			}
		}

		void BinaryToLexer(LexerData& out, const std::vector<u64>& in, typename std::vector<u64>::const_iterator& it) {
			u64 tableSize = *it; it++;
			u64 endDataSize = *it; it++;

			u64** table = new u64 * [tableSize];
			for (size_t i = 0; i < tableSize; i++) {
				table[i] = new u64[128];
				for (size_t j = 0; j < 128; j++) {
					table[i][j] = *it;
					it++;
				}
			}

			std::vector<std::array<u64, 3>> endData;
			for (size_t i = 0; i < endDataSize; i++) {
				std::array<u64, 3> tmp;
				for (size_t j = 0; j < 3; j++) {
					tmp[j] = *it;
					it++;
				}
				endData.push_back(tmp);
			}

			out.table.size = tableSize;
			out.table.table = table;
			for (size_t i = 0; i < endDataSize; i++)
				out.endData.push_back({ endData[i][0], { (lexer::TokenType)endData[i][1], endData[i][2] } });
		}

		void LexerToBinary(std::vector<u64>& out, const LexerData& in) {
			u64 tableSize = in.table.size;
			u64 endDataSize = in.endData.size();

			std::vector<std::array<u64, 3>> endData;
			for (auto it = in.endData.cbegin(); it != in.endData.cend(); ++it)
				endData.push_back({ (u64)it->first, (u64)it->second.token, (u64)it->second.priority });

			out.push_back(tableSize);
			out.push_back(endDataSize);
			for (size_t i = 0; i < tableSize; i++) {
				for (size_t j = 0; j < 128; j++)
					out.push_back(in.table.table[i][j]);
			}
			for (auto it = endData.cbegin(); it != endData.cend(); ++it) {
				out.push_back((*it)[0]);
				out.push_back((*it)[1]);
				out.push_back((*it)[2]);
			}
		}

		void BinaryToParser(ParserData& out, const std::vector<u64>& in, typename std::vector<u64>::const_iterator& it) {
			u64 tableSize = *it; it++;
			u64 actionTableSize = *it; it++;
			u64 gotoTableSize = *it; it++;
			u64 vectorSize = *it; it++;
			std::vector<u64> vectorTailSize;
			for (size_t i = 0; i < vectorSize; i++) {
				vectorTailSize.push_back(*it);
				it++;
			}

			out.table.size = tableSize;
			out.table.actionSize = actionTableSize;
			out.table.gotoSize = gotoTableSize;

			out.table.actionTable = new parser_generator::convert::Action * [tableSize];
			for (size_t i = 0; i < tableSize; i++) {
				out.table.actionTable[i] = new parser_generator::convert::Action[actionTableSize];
				for (size_t j = 0; j < actionTableSize; j++) {
					out.table.actionTable[i][j].arg = *it; it++;
					out.table.actionTable[i][j].type = (parser_generator::convert::ActionType)*it; it++;
				}
			}

			out.table.gotoTable = new parser_generator::convert::Goto * [tableSize];
			for (size_t i = 0; i < tableSize; i++) {
				out.table.gotoTable[i] = new parser_generator::convert::Goto[gotoTableSize];
				for (size_t j = 0; j < gotoTableSize; j++) {
					out.table.gotoTable[i][j].state = *it; it++;
				}
			}

			for (size_t i = 0; i < tableSize; i++) {
				int count = 0;
				for (size_t j = 0; j < gotoTableSize; j++) {
					out.table.gotoTable[i][j].error = (*it >> (63 - count)) & 1;
					count++; count %= 64;
					if (!count)
						it++;
				}
				if (count) 
					it++;
			}

			for (size_t i = 0; i < vectorSize; i++) {
				u64 head = *it; it++;
				std::vector<u64> tmp;
				for (size_t j = 0; j < vectorTailSize[i]; j++) {
					tmp.push_back(*it); it++;
				}
				out.grammerVec.push_back({ head, tmp });
			}

			for (size_t i = 0; i < vectorSize; i++) {
				u64 head1 = *it; it++;
				u64 head2 = *it; it++;
				std::vector<std::pair<bool, bool>> tmp;
				int count = 0;
				for (size_t j = 0; j < vectorTailSize[i]; j++) {
					bool first = (*it >> (63 - count)) & 1; count++;
					bool second = (*it >> (63 - count)) & 1; count++;
					tmp.push_back({ first, second });
					count %= 32;
					if (!count)
						it++;
				}
				if (count) 
					it++;
				out.ASTActionVec.push_back({ {head1, head2}, tmp });
			}
		}

		void ParserToBinary(std::vector<u64>& out, const ParserData& in) {
			u64 tableSize = in.table.size;
			u64 actionTableSize = in.table.actionSize;
			u64 gotoTableSize = in.table.gotoSize;
			u64 vectorSize = in.grammerVec.size();
			std::vector<u64> vectorTailSize;
			for (auto it = in.grammerVec.cbegin(); it != in.grammerVec.cend(); ++it)
				vectorTailSize.push_back(it->second.size());

			std::vector<u64> grammerVec;
			std::vector<u64> ASTActionVec;

			for (auto it = in.grammerVec.cbegin(); it != in.grammerVec.cend(); ++it) {
				grammerVec.push_back(it->first);
				u64 tmp = 0;
				int count = 0;
				for (auto si = it->second.cbegin(); si != it->second.cend(); ++si)
					grammerVec.push_back(*si);
			}

			for (auto it = in.ASTActionVec.cbegin(); it != in.ASTActionVec.cend(); ++it) {
				ASTActionVec.push_back(it->first.first);
				ASTActionVec.push_back(it->first.second);
				u64 tmp = 0;
				int count = 0;
				for (auto si = it->second.cbegin(); si != it->second.cend(); ++si) {
					tmp = (tmp << 1) + (si->first ? 1 : 0);
					tmp = (tmp << 1) + (si->second ? 1 : 0);
					count++; count %= 32;
					if (!count) {
						ASTActionVec.push_back(tmp);
						tmp = 0;
					}
				}
				if (count) {
					tmp = tmp << (2 * (32 - count));
					ASTActionVec.push_back(tmp);
				}
			}

			out.push_back(tableSize);
			out.push_back(actionTableSize);
			out.push_back(gotoTableSize);
			out.push_back(vectorSize);
			for (size_t i = 0; i < vectorSize; i++)
				out.push_back(vectorTailSize[i]);

			for (size_t i = 0; i < tableSize; i++) {
				for (size_t j = 0; j < actionTableSize; j++) {
					out.push_back(in.table.actionTable[i][j].arg);
					out.push_back((u64)in.table.actionTable[i][j].type);
				}
			}

			for (size_t i = 0; i < tableSize; i++) {
				for (size_t j = 0; j < gotoTableSize; j++) {
					out.push_back(in.table.gotoTable[i][j].state);
				}
			}

			for (size_t i = 0; i < tableSize; i++) {
				u64 tmp = 0;
				int count = 0;
				for (size_t j = 0; j < gotoTableSize; j++) {
					tmp = (tmp << 1) + (in.table.gotoTable[i][j].error ? 1 : 0);
					count++; count %= 64;
					if (!count) {
						out.push_back(tmp);
						tmp = 0;
					}
				}
				if (count) {
					tmp = tmp << (64 - count);
					out.push_back(tmp);
				}
			}

			for (auto it = grammerVec.cbegin(); it != grammerVec.cend(); ++it)
				out.push_back(*it);
			for (auto it = ASTActionVec.cbegin(); it != ASTActionVec.cend(); ++it)
				out.push_back(*it);
		}

		void clearLexerData(LexerData& in) {
			if (in.table.table != NULL) {
				for (size_t i = 0; i < in.table.size; i++)
					in.table.table[i];
				delete[] in.table.table;
			}
			in.table.size = 0;
			in.table.table = NULL;

			in.endData.clear();
		}

		void clearParserData(ParserData& in) {
			if (in.table.actionTable != NULL) {
				for (size_t i = 0; i < in.table.size; i++)
					delete[] in.table.actionTable[i];
				delete[] in.table.actionTable;
			}
			if (in.table.gotoTable != NULL) {
				for (size_t i = 0; i < in.table.size; i++)
					delete[] in.table.gotoTable[i];
				delete[] in.table.gotoTable;
			}
			in.table.size = 0;
			in.table.actionSize = 0;
			in.table.gotoSize = 0;
			in.table.actionTable = NULL;
			in.table.gotoTable = NULL;

			in.ASTActionVec.clear();
			in.grammerVec.clear();
		}

		u64 BinaryToASM(ParserData& parseOut, LexerData& lexOut, const std::vector<u8>& in) {
			std::vector<u64> binary;
			pack(binary, in);
			auto it = binary.cbegin();
			u64 version = *it; it++;

			if (version != assembler::version)
				return version;

			BinaryToLexer(lexOut, binary, it);
			BinaryToParser(parseOut, binary, it);

			return version;
		}

		void ASMToBinary(std::vector<u8>& out, const ParserData& parseIn, const LexerData& lexIn) {
			std::vector<u64> binary;
			binary.push_back(version);
			LexerToBinary(binary, lexIn);
			ParserToBinary(binary, parseIn);
			unpack(out, binary);
		}
	}

	using u8 = unsigned __int8;
	using u32 = unsigned __int32;
	using u64 = unsigned __int64;

	void createAssembler(void) {
		std::vector<u8> buff;
		std::vector<u8> temp;
		if (iotools::readBinaryFile(iotools::directory + iotools::executable + "_table.bin", buff)) {
			if (buff.size() < 8)
				goto createDefault;

			cache::LexerData lexData;
			cache::ParserData parseData;
			temp.reserve(buff.size());
			cache::decompress(temp, buff);
			u64 fileVer = cache::BinaryToASM(parseData, lexData, temp);

			if (fileVer == assembler::version) {
				lexer::createLexer(lexData);
				parser::createParser(parseData);

				cache::clearLexerData(lexData);
				cache::clearParserData(parseData);
				return;
			}
		}
		
	createDefault:
		lexer::createLexer();
		parser::createParser();

		cache::LexerData lexData;
		cache::ParserData parseData;

		lexer::getFactoryData(lexData);
		parser::getFactoryData(parseData);
		buff.clear();
		temp.clear();

		cache::ASMToBinary(buff, parseData, lexData);
		temp.reserve(buff.size());
		cache::compress(temp, buff);
		iotools::writeBinaryFile(iotools::directory + iotools::executable + "_table.bin", temp);

		cache::clearLexerData(lexData);
		cache::clearParserData(parseData);
	}

	typedef struct _AssemblerDump {
		enum flag {
			none = 0,
			getToken = 1 << 0,
			getPreproc = 1 << 1,
			getParse = 1 << 2,
			getAST = 1 << 3,
			getEval = 1 << 4,
			getBin = 1 << 5
		};

		u32 flags;

		bool parseSuccess, preprocSuccess, evalSuccess;
		std::vector<lexer::Token> tokens;
		std::vector<preprocesser::Token> preprocTokens;
		std::string errorMessage;
		parser::Parser::ASTNode* pAST;
		std::vector<u8> textSection;
		std::vector<u8> dataSection;
		std::vector<u8> bssSection;
	} AssemblerDump;

	int assemble(const std::string& text, std::vector<u8>& binary, AssemblerDump& dump) {
		using TokenType = lexer::TokenType;
		using Token = lexer::Token;
		using Parser = parser::Parser;

		std::vector<Token> tokens;
		lexer::lexer.lex(tokens, text);

		if (dump.flags & AssemblerDump::getToken) {
			dump.tokens = tokens;
		}

		preprocesser::functions::PreprocData preprocData;
		preprocesser::setTokens(tokens);
		bool validPreprocess = preprocesser::preprocess(preprocData);

		if (dump.flags & AssemblerDump::getPreproc) {
			dump.preprocTokens = tokens;
			dump.preprocSuccess = validPreprocess;
		}

		if (!validPreprocess) {
			return -1;
		}

		Parser::ASTNode* pAST;
		bool validParse = parser::parser.parse(pAST, tokens);

		if (dump.flags & AssemblerDump::getAST) {
			dump.parseSuccess = validParse;
			dump.pAST = pAST;
		}

		if (dump.flags & AssemblerDump::getEval) {
			dump.parseSuccess = validParse;
			if (!dump.parseSuccess)
				dump.errorMessage = pAST->child[0]->text;
		}

		if (!validParse) {
			parser::parser.delAST(pAST);
			return -2;
		}

		evaluator::functions::EvalData data;
		evaluator::setTree(pAST);
		bool validEvaluate = evaluator::evaluate(data);

		if (dump.flags & AssemblerDump::getEval) {
			dump.evalSuccess = validEvaluate;
		}

		if (!validEvaluate) {
			parser::parser.delAST(pAST);
			return -3;
		}

		if (dump.flags & AssemblerDump::getBin) {
			dump.textSection = data.textSection;
			dump.dataSection = data.dataSection;
			dump.bssSection = data.bssSection;
		}

		parser::parser.delAST(pAST);
		binary = data.rawData;

		return 0;
	}
}

#endif