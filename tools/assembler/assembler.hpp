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
#include <algorithm>

#include "lexer.hpp"
#include "preprocesser.hpp"
#include "parser.hpp"
#include "evaluator.hpp"

#include "../common/format.hpp"
#include "../common/iotool.hpp"

namespace assembler {
	// byte order of the emitted code/data (dasm -big / -little, default little); also recorded
	// in the object file so the linker patches relocations in the same order
	bool bigEndian = false;

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

			block,

			jmp, ijmp,
			call, ret,
			nop, brk,
			wait, halt,

			syscall, iret,
			intervec, interpri,
			stexp, ldexp,
			pctl, getabs,
			flush, inval,
			atom,

			cpuid, panicvec, istack, intlevel, pagebase, plimit,
			enable, disable,
			L1D, L1I, TLB, L2, ALL,

			reg,
			gen,
			sbp, zero, one, full, pc, stack, flag,

			neg, pos, carry, carry4, overflow,

			_32B_, _16L_, _8L_, _8H_, _S16H_, _S16L_, _S8L_, _S8H_,

			text, data, bss,

			global,
			_extern,
			equ,
			section,

			include,
			ifndef,
			ifdef,

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
			less, greater, lessequal, greaterequal, equal, notequal,
			lognot, logampersend, logverticalbar, selector,

			curline,

			comment,

			whitespace,
			newline,

			character,
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
				{ "block", TokenType::block },
				{ "push", TokenType::push },
				{ "pop", TokenType::pop },
				{ "ld", TokenType::ld },
				{ "st", TokenType::st },
				{ "jmp", TokenType::jmp },
				{ "ijmp", TokenType::ijmp },
				{ "call", TokenType::call },
				{ "ret", TokenType::ret },
				{ "nop", TokenType::nop },
				{ "brk", TokenType::brk },
				{ "wait", TokenType::wait },
				{ "halt", TokenType::halt },

				{ "syscall", TokenType::syscall},
				{ "iret", TokenType::iret},
				{ "intervec", TokenType::intervec},
				{ "interpri", TokenType::interpri},

				{ "stexp", TokenType::stexp },
				{ "ldexp", TokenType::ldexp },
				{ "pctl", TokenType::pctl },
				{ "getabs", TokenType::getabs },
				{ "flush", TokenType::flush },
				{ "inval", TokenType::inval },
				{ "atom", TokenType::atom },

				{ "istack", TokenType::istack },
				{ "panicvec", TokenType::panicvec },
				{ "cpuid", TokenType::cpuid },
				{ "intlevel", TokenType::intlevel },
				{ "pagebase", TokenType::pagebase },
				{ "pbase", TokenType::pagebase },
				{ "plimit", TokenType::plimit },
				{ "pagelimit", TokenType::plimit },
				{ "enable", TokenType::enable },
				{ "disable", TokenType::disable },
				{ "L1D", TokenType::L1D },
				{ "L1I", TokenType::L1I },
				{ "TLB", TokenType::TLB },
				{ "L2", TokenType::L2 },
				{ "ALL", TokenType::ALL },

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

				{ "global", TokenType::global },
				{ "extern", TokenType::_extern },
				{ "equ", TokenType::equ },
				{ "section", TokenType::section },

				{ "#include<[^>\t\n]*>", TokenType::include },
				{ "#ifndef", TokenType::ifndef },
				{ "#ifdef", TokenType::ifdef },

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

				{ "\\+", TokenType::plus },
				{ "-", TokenType::minus },
				{ "\\*\\*", TokenType::dstar },
				{ "\\*", TokenType::star },
				{ "/", TokenType::slash },
				{ "%", TokenType::percent },
				{ "~", TokenType::tilde },
				{ "&&", TokenType::logampersend },
				{ "&", TokenType::ampersend },
				{ "\\|\\|", TokenType::logverticalbar },
				{ "\\|", TokenType::verticalbar },
				{ "^", TokenType::caret },
				{ "<<", TokenType::leftshift },
				{ ">>", TokenType::rightshift },
				{ "<=", TokenType::lessequal },
				{ ">=", TokenType::greaterequal },
				{ "<" , TokenType::less},
				{ ">" , TokenType::greater},
				{ "==", TokenType::equal },
				{ "!=", TokenType::notequal },
				{ "!", TokenType::lognot },
				{ "\\?", TokenType::selector },

				{ "$", TokenType::curline}
			},
			{
				{ "0x[0-9a-fA-F]+", TokenType::hexnum },
				{ "[0-9]+", TokenType::decnum },
				{ "0o[0-7]+", TokenType::octnum },
				{ "0b[01]+", TokenType::binnum },
 
				{ "#[0-9]+", TokenType::macro_arg },

				{ "[a-zA-Z_][a-zA-Z0-9_]*", TokenType::identifier },
				{ "##[a-zA-Z_][a-zA-Z0-9_]*", TokenType::identifier_unique },

				{ "\'([^\\\\\n\t\']|(\\\\([nt\\\\]|([0-9]{1,3}))))\'", TokenType::character},
				{ "\"([^\\\\\"\n]|\\\\(.|\n))*\"", TokenType::string },

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
			{ TokenType::rol, "rol" },
			{ TokenType::roli, "roli" },
			{ TokenType::ror, "ror" },
			{ TokenType::rori, "rori" },
			{ TokenType::cmp, "cmp" },
			{ TokenType::cmpi, "cmpi" },
			{ TokenType::test, "test" },
			{ TokenType::testi, "testi" },
			{ TokenType::mov, "mov" },
			{ TokenType::set, "set" },
			{ TokenType::sets, "sets" },
			{ TokenType::block, "block" },
			{ TokenType::push, "push" },
			{ TokenType::pop, "pop" },
			{ TokenType::ld, "ld" },
			{ TokenType::st, "st" },
			{ TokenType::jmp, "jmp" },
			{ TokenType::ijmp, "ijmp" },
			{ TokenType::call, "call" },
			{ TokenType::ret, "ret" },
			{ TokenType::nop, "nop" },
			{ TokenType::brk, "brk" },
			{ TokenType::wait, "wait" },
			{ TokenType::halt, "halt" },

			{ TokenType::syscall, "syscall" },
			{ TokenType::iret, "iret" },
			{ TokenType::intervec, "intervec" },
			{ TokenType::interpri, "interpri" },

			{ TokenType::stexp, "stexp" },
			{ TokenType::ldexp, "ldexp" },
			{ TokenType::pctl, "pctl" },
			{ TokenType::getabs, "getabs" },
			{ TokenType::flush, "flush" },
			{ TokenType::inval, "inval" },
			{ TokenType::atom, "atom" },

			{ TokenType::istack, "istack" },
			{ TokenType::panicvec, "panicvec" },
			{ TokenType::cpuid, "cpuid" },
			{ TokenType::intlevel, "intlevel" },
			{ TokenType::pagebase, "pagebase" },
			{ TokenType::plimit, "plimit" },
			{ TokenType::enable, "enable" },
			{ TokenType::disable, "disable" },
			{ TokenType::L1D, "L1D" },
			{ TokenType::L1I, "L1I" },
			{ TokenType::TLB, "TLB" },
			{ TokenType::L2, "L2" },
			{ TokenType::ALL, "ALL" },

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

			{ TokenType::global, "global" },
			{ TokenType::_extern, "extern" },
			{ TokenType::equ, "equ" },
			{ TokenType::section, "section" },

			{ TokenType::include, "include" },
			{ TokenType::ifndef, "ifndef" },
			{ TokenType::ifdef, "ifdef" },

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

			{ TokenType::less, "less" },
			{ TokenType::greater, "greater" },
			{ TokenType::lessequal, "lessequal" },
			{ TokenType::greaterequal, "greaterequal" },
			{ TokenType::equal, "equal" },
			{ TokenType::notequal, "notequal" },
			{ TokenType::lognot, "lognot" },
			{ TokenType::logampersend, "logampersend" },
			{ TokenType::logverticalbar, "logverticalbar" },
			{ TokenType::selector, "selector" },

			{ TokenType::curline, "curline" },

			{ TokenType::hexnum, "hexnum" },
			{ TokenType::decnum, "decnum" },
			{ TokenType::octnum, "octnum" },
			{ TokenType::binnum, "binnum" },
			{ TokenType::comment, "comment" },
			{ TokenType::whitespace, "whitespace" },

			{ TokenType::newline, "newline" },
			{ TokenType::identifier, "identifier" },
			{ TokenType::character, "character" },
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

			bool includePath(std::string& path, std::string text) {
				path.clear();

				auto it = text.begin();
				// "#include<...>"
				for (; it != text.end() && *it != '<'; ++it);
				if (it == text.end())
					return false;
				++it;

				for (; it != text.end() && *it != '>'; ++it)
					path.push_back(*it);
				if (it == text.end())
					return false;
				++it;
				if (it != text.end())
					return false;
				return true;
			}

			enum class State {
				normal,

				readDefine_identifier,
				readDefine_token,

				readMacro_identifier,
				readMacro_argCount,
				readMacro_token,

				readInclude,
				readIf,

				replace_identifier,
				remove
			};

			// PreprocessFunction return codes: 0 = success, anything else is propagated
			// unchanged by Preprocesser::preprocess()/preprocesser::preprocess() up to assemble().
			enum class PreprocessError {
				SUCCESS = 0,
				MACRO_ARG_OUT_OF_RANGE,   // #macro invoked with fewer args than a #<n> placeholder refers to
				EXPECTED_DEFINE_IDENTIFIER, // #define not followed by an identifier
				EXPECTED_MACRO_IDENTIFIER,  // #macro not followed by an identifier
				EXPECTED_MACRO_ARGCOUNT,     // #macro identifier not followed by a decimal arg count
				INVALID_INCLUDE_PATH,	// #include<...>'s path is not valid
				DUPLICATE_INCLUDE,     // #include<...> resolves to a file already included (catches cycles too)
				EXPECTED_IF_IDENTIFIER,  // #ifdef/#ifndef not followed by an identifier
				UNEXPECTED_END,          // #end with no open #ifdef/#ifndef (or #macro)
				UNTERMINATED_IF,         // #ifdef/#ifndef block still open at the end of the input
				UNTERMINATED_DIRECTIVE,  // #define/#macro/macro call still incomplete at the end of the input
				MACRO_ARG_COUNT_MISMATCH, // macro called with a different number of arguments than #macro declared
				EXPECTED_MACRO_CALL      // macro that takes arguments used without NAME(...)
			};

			inline const char* preprocessErrorName(int code) {
				switch ((PreprocessError)code) {
				case PreprocessError::SUCCESS: return "success";
				case PreprocessError::MACRO_ARG_OUT_OF_RANGE: return "macro argument out of range";
				case PreprocessError::EXPECTED_DEFINE_IDENTIFIER: return "expected identifier after #define";
				case PreprocessError::EXPECTED_MACRO_IDENTIFIER: return "expected identifier after #macro";
				case PreprocessError::EXPECTED_MACRO_ARGCOUNT: return "expected argument count after #macro name";
				case PreprocessError::INVALID_INCLUDE_PATH: return "invalid include path";
				case PreprocessError::DUPLICATE_INCLUDE: return "duplicate include";
				case PreprocessError::EXPECTED_IF_IDENTIFIER: return "expected identifier after #ifdef/#ifndef";
				case PreprocessError::UNEXPECTED_END: return "#end without matching #ifdef/#ifndef";
				case PreprocessError::UNTERMINATED_IF: return "#ifdef/#ifndef without matching #end";
				case PreprocessError::UNTERMINATED_DIRECTIVE: return "unterminated directive";
				case PreprocessError::MACRO_ARG_COUNT_MISMATCH: return "wrong number of macro arguments";
				case PreprocessError::EXPECTED_MACRO_CALL: return "expected ( after macro name";
				}
				return "unknown error";
			}

			typedef struct _PreprocData {
				State state;

				std::string curReplaceString;
				std::vector<Token> curReplaceTokens;
				int curMacroArgCount;
				int depth;
				std::unordered_map<std::string, std::pair<std::vector<Token>, int>> replaceTable;
				bool invert = false;

				// opening #ifdef/#ifndef tokens of the blocks currently open (kept for error location)
				std::vector<Token> ifOpen;
				// directives opened inside a block being removed, so their #end isn't taken as the block's
				int removeDepth = 0;

				std::vector<Token> curArg;
				std::vector<std::vector<Token>> args;
				int remainingArg;
				
				std::vector<Token> replaceTokens;
				std::string fileName;
				std::unordered_set<std::string> includedFiles;

				int uniqueCounter = 0;
			} PreprocData;

			template<class FuncSig> class FunctionRef;

			// Despite the name (kept as-is), this OWNS a heap copy of whatever callable it's given
			// rather than just referencing it -- it's assigned a temporary lambda (see
			// includeCallback below), which only lives through the end of that one statement, so a
			// non-owning reference would dangle the moment the assignment completes. Copy ctor/
			// assignment are deleted rather than given shallow-copy semantics, since a shallow copy
			// of an owning pointer would double-free on destruction; nothing currently copies a
			// FunctionRef, only assigns a callable into one.
			template<class Ret, class... Arg>
			class FunctionRef<Ret(Arg...)> {
			private:
				void* object;
				Ret(*call)(void*, Arg...);
				void(*deleter)(void*);

				void reset() {
					if (deleter) deleter(object);
					object = NULL;
					call = NULL;
					deleter = NULL;
				}

			public:
				FunctionRef(void) : object(NULL), call(NULL), deleter(NULL) {}

				FunctionRef(const FunctionRef&) = delete;
				FunctionRef& operator=(const FunctionRef&) = delete;

				~FunctionRef() { reset(); }

				template<class Func>
				FunctionRef& operator=(const Func& func) {
					reset();
					object = new Func(func);
					call = [](void* obj, Arg... arg) -> Ret { return (*(Func*)obj)(arg...); };
					deleter = [](void* obj) { delete (Func*)obj; };
					return *this;
				}

				Ret operator()(Arg... arg) const {
					return call(object, arg...);
				}
			};

			FunctionRef<int(std::vector<lexer::Token>&, std::string, PreprocData*)> includeCallback;

			// Substitutes the collected args into data->replaceTokens and inserts the result at `it`
			// (it then points at the first inserted token, so the expansion is preprocessed too).
			int expandMacro(Vec tokens, Iter it, PreprocData* data) {
				for (auto si = data->replaceTokens.begin(); si != data->replaceTokens.end(); ) {
					if (si->type == TokenType::macro_arg) {
						int id = macroArg2int(si->text) - 1;
						if (id < 0 || id >= (int)data->args.size())
							return (int)PreprocessError::MACRO_ARG_OUT_OF_RANGE;
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
				return (int)PreprocessError::SUCCESS;
			}

			int handleReplace(Vec tokens, Iter it, void* vpdata) {
				PreprocData* data = (PreprocData*)vpdata;

				switch (data->state) {
				case State::replace_identifier: {
					// the call must start with '(' (a 0-arg macro without one was already
					// expanded in State::normal)
					if (data->depth == 0 && it->type != TokenType::openparen)
						return (int)PreprocessError::EXPECTED_MACRO_CALL;

					bool complete = false;
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
							// last argument; NAME() has none (no comma seen and nothing collected)
							if (!data->args.empty() || !data->curArg.empty()) {
								data->args.push_back(data->curArg);
								data->curArg.clear();
								data->remainingArg--;
							}
							if (data->remainingArg != 0)
								return (int)PreprocessError::MACRO_ARG_COUNT_MISMATCH;
							complete = true;
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
					if (complete) {
						int code = expandMacro(tokens, it, data);
						if (code) return code;
					}
					break;
				}
				
				case State::remove:
					switch (it->type) {
					case TokenType::ifdef:
					case TokenType::ifndef:
					case TokenType::macro:
						data->removeDepth++;
						break;
					case TokenType::end:
						if (data->removeDepth)
							data->removeDepth--;
						else {
							data->ifOpen.pop_back();
							data->state = State::normal;
						}
						break;
					default:
						break;
					}
					it = tokens.erase(it);
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

						data->state = State::replace_identifier;
						data->replaceTokens = find->second.first;
						data->remainingArg = find->second.second;
						data->args.clear();
						data->curArg.clear();
						data->depth = 0;

						// a 0-arg macro may be written without "()": expand now instead of
						// taking the next token as its argument list
						if (data->remainingArg == 0 && (it == tokens.end() || it->type != TokenType::openparen)) {
							int code = expandMacro(tokens, it, data);
							if (code) return code;
						}

						break;
					}

					case TokenType::include: {
						std::string path = "";
						if (!includePath(path, it->text))
							return (int)PreprocessError::INVALID_INCLUDE_PATH;
						int code = includeCallback(data->replaceTokens, path, data);
						if (code) return code;

						// erase and splice in the SAME call (state stays normal): if the
						// #include<...> happened to be the last token in the (possibly shrunk)
						// vector, erase() would return end(), and the generic preprocess loop's
						// `it != tokens->end()` check would exit immediately -- a deferred
						// "insert on the next call" would then never run, silently dropping the
						// whole included file's content. Doing both here means `it` always ends
						// up on the first inserted token (or wherever came after, if the include
						// resolved to nothing), so nested #include/#define within it still get
						// picked up by the same ongoing pass.
						it = tokens.erase(it);
						it = tokens.insert(it, data->replaceTokens.begin(), data->replaceTokens.end());
						break;
					}

					case TokenType::ifndef:
					case TokenType::ifdef:
						data->invert = (it->type == TokenType::ifndef);
						data->ifOpen.push_back(*it);
						data->state = State::readIf;
						it = tokens.erase(it);
						break;

					// closes a #ifdef/#ifndef whose condition was true (a #macro's #end is
					// consumed in readMacro_token, a removed block's in remove)
					case TokenType::end:
						if (data->ifOpen.empty())
							return (int)PreprocessError::UNEXPECTED_END;
						data->ifOpen.pop_back();
						it = tokens.erase(it);
						break;

					default:
						it++;
						break;
					}
					break;

				case State::readIf: {
					if (it->type != TokenType::identifier)
						return (int)PreprocessError::EXPECTED_IF_IDENTIFIER;

					bool found = data->replaceTable.find(it->text) != data->replaceTable.end();
					if (data->invert ? found : !found) {
						data->state = State::remove;
						data->removeDepth = 0;
					}
					else
						data->state = State::normal;

					it = tokens.erase(it);
					break;
				}

				case State::readDefine_identifier:
					if (it->type == TokenType::identifier) {
						data->state = State::readDefine_token;
						data->curReplaceString = it->text;
						data->depth = 0;
						it = tokens.erase(it);
					}
					else
						return (int)PreprocessError::EXPECTED_DEFINE_IDENTIFIER;
					break;

				case State::readDefine_token:
					switch (it->type) {
					case TokenType::openparen:
						data->depth++;
						data->curReplaceTokens.push_back(*it);
						it = tokens.erase(it);
						break;
					case TokenType::closeparen:
						data->depth--;
						data->curReplaceTokens.push_back(*it);
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
						return (int)PreprocessError::EXPECTED_MACRO_IDENTIFIER;
					break;
				case State::readMacro_argCount:
					if (it->type == TokenType::decnum) {
						data->state = State::readMacro_token;
						data->curMacroArgCount = dec2int(it->text);
						data->removeDepth = 0;
						it = tokens.erase(it);
					}
					else
						return (int)PreprocessError::EXPECTED_MACRO_ARGCOUNT;
					break;
				case State::readMacro_token:
					switch (it->type) {
					// #ifdef/#ifndef blocks inside the body are kept as body tokens (evaluated
					// when the macro expands); only the #end at depth 0 closes the macro
					case TokenType::ifdef:
					case TokenType::ifndef:
						data->removeDepth++;
						data->curReplaceTokens.push_back(*it);
						it = tokens.erase(it);
						break;
					case TokenType::end:
						if (data->removeDepth) {
							data->removeDepth--;
							data->curReplaceTokens.push_back(*it);
							it = tokens.erase(it);
							break;
						}
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

				return (int)PreprocessError::SUCCESS;
			}

			int removeUnused(Vec tokens, Iter it, void* vpdata) {
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

				return (int)PreprocessError::SUCCESS;
			}
		}

		inline int tagLine(std::vector<lexer::Token>& tokens, int line = 1, int file = 0) {
			int relLine = 1;
			for (auto it = tokens.begin(); it != tokens.end(); ++it) {
				it->line = line;
				it->fileLine = relLine;
				it->fileId = file;
				if (it->type == lexer::TokenType::newline) {
					line++;
					relLine++;
				}
			}
			return line;
		}

		// Maps an absolute (cross-file) line number back to the file it came from and the line
		// within that file. absLine is unique per source line across the whole concatenated
		// multi-file token stream (see tagLine() above), so backupTokens - the pre-preprocessing
		// token list, which still has every line intact - always has at least one token carrying
		// it. Returns false if absLine isn't present (e.g. out of range).
		inline bool resolveSourceLocation(const std::multimap<int, lexer::Token>& backupTokens, int absLine, int& fileId, int& fileLine) {
			auto it = backupTokens.find(absLine);
			if (it != backupTokens.end()) {
				fileId = it->second.fileId;
				fileLine = it->second.fileLine;
				return true;
			}
			return false;
		}

		// Lexically collapses "." and ".." path segments (e.g. "A/B/../C" -> "A/C",
		// "A/./C" -> "A/C") without touching the filesystem, so textually different but
		// equivalent paths (e.g. reached via different #include<..> detours) compare equal
		// when checked against includedFiles for duplicate/circular #include detection.
		// Accepts either '/' or '\\' as a separator and always re-joins with '\\' to match
		// the backslash-separated absolute paths iotools::absolutePath() produces.
		inline std::string normalizePath(const std::string& path) {
			std::string prefix;
			std::string rest = path;

			size_t colon = path.find(':');
			if (colon != std::string::npos) {
				prefix = path.substr(0, colon + 1);
				rest = path.substr(colon + 1);
			}

			std::vector<std::string> segments;
			std::string cur;
			for (size_t i = 0; i <= rest.size(); i++) {
				if (i == rest.size() || rest[i] == '/' || rest[i] == '\\') {
					if (cur == "..") {
						if (!segments.empty())
							segments.pop_back();
					}
					else if (!cur.empty() && cur != ".") {
						segments.push_back(cur);
					}
					cur.clear();
				}
				else {
					cur.push_back(rest[i]);
				}
			}

			std::string result = prefix;
			for (auto it = segments.cbegin(); it != segments.cend(); ++it) {
				result.push_back('\\');
				result += *it;
			}
			return result;
		}

		Preprocesser preprocesserRemove;
		Preprocesser preprocesserReplace;

		inline void setTokensRem(std::vector<lexer::Token>& tokens) {
			preprocesserRemove.setTokens(tokens);
		}
		
		inline int preprocessRem(lexer::Token& errorTok, functions::PreprocData& data) {
			int code = preprocesserRemove.preprocess(errorTok, functions::removeUnused, &data);
			if (code) return code;

			return (int)functions::PreprocessError::SUCCESS;
		}

		inline void setTokensRep(std::vector<lexer::Token>& tokens) {
			preprocesserReplace.setTokens(tokens);
		}

		inline int preprocessRep(lexer::Token& errorTok, functions::PreprocData& data) {
			int code = preprocesserReplace.preprocess(errorTok, functions::handleReplace, &data);
			if (code) return code;

			// whole input consumed with a directive still open
			if (!data.ifOpen.empty()) {
				errorTok = data.ifOpen.back();
				return (int)functions::PreprocessError::UNTERMINATED_IF;
			}
			if (data.state != functions::State::normal) {
				errorTok = {};
				errorTok.line = -1;
				return (int)functions::PreprocessError::UNTERMINATED_DIRECTIVE;
			}

			return (int)functions::PreprocessError::SUCCESS;
		}
	}

	namespace parser {
		using u64 = unsigned __int64;

		using TokenType = lexer::TokenType;

		NTSTART(NonterminalType)
			program,
			section,
			section_unit,

			text_section,
			data_section,
			bss_section,
			outer_section,

			intruction,
			label,
			equate,
			global,
			_extern,

			allocate,
			allocate_list,
			allocate_string,
			allocate_zero,

			list,
			list_tail,

			reg_ex,

			reg,
			regid,
			regmode,
			reg_gen,
			reg_reg,

			flagid,

			ctlid,
			cacheid,

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
			expressionL9,
			expressionL10,
			expressionL11,
			expressionL12,
			expressionL13,

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
			allocate_list,
			allocate_string,
			allocate_zero,

			list,

			instruction_N,
			instruction_R,
			instruction_RR,
			instruction_RI,
			instruction_II,
			instruction_RII,
			instruction_IRI,
			instruction_RVRI,
			instruction_FVRI,

			instruction_IXP,
			instruction_RXR,
			instruction_CTL,
			instruction_CACHE,

			label,
			equate,
			global,
			global_section,
			_extern,

			reg_ex,

			reg,
			regid,
			regmode,
			reg_index,

			flagid,

			ctlid,
			cacheid,

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
			{ NonterminalType::section_unit, "section_unit" },

			{ NonterminalType::text_section, "text_section" },
			{ NonterminalType::data_section, "data_section" },
			{ NonterminalType::bss_section, "bss_section" },
			{ NonterminalType::outer_section, "outer_section" },

			{ NonterminalType::intruction, "intruction" },
			{ NonterminalType::label, "label" },
			{ NonterminalType::equate, "equate" },
			{ NonterminalType::global, "global" },
			{ NonterminalType::_extern, "extern" },
			{ NonterminalType::allocate, "allocate" },
			{ NonterminalType::allocate_list, "allocate_list" },
			{ NonterminalType::allocate_string, "allocate_string" },
			{ NonterminalType::allocate_zero, "allocate_zero" },
			{ NonterminalType::list, "list" },
			{ NonterminalType::list_tail, "list_tail" },

			{ NonterminalType::reg_ex, "reg_ex"},

			{ NonterminalType::reg, "reg" },
			{ NonterminalType::regid, "regid" },
			{ NonterminalType::regmode, "regmode" },
			{ NonterminalType::reg_gen, "reg_gen" },
			{ NonterminalType::reg_reg, "reg_reg" },

			{ NonterminalType::flagid, "flagid" },

			{ NonterminalType::ctlid, "ctlid" },
			{ NonterminalType::cacheid, "cacheid" },

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
			{ NonterminalType::expressionL9, "expressionL9" },
			{ NonterminalType::expressionL10, "expressionL10" },
			{ NonterminalType::expressionL11, "expressionL11" },
			{ NonterminalType::expressionL12, "expressionL12" },
			{ NonterminalType::expressionL13, "expressionL13" },

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
			{ ASTNodeType::instruction_RII, "instruction_RII" },
			{ ASTNodeType::instruction_IRI, "instruction_IRI" },
			{ ASTNodeType::instruction_RVRI, "instruction_RVRI" },
			{ ASTNodeType::instruction_FVRI, "instruction_FVRI" },
			{ ASTNodeType::instruction_CTL, "instruction_CTL" },
			{ ASTNodeType::instruction_CACHE, "instruction_CACHE" },

			{ ASTNodeType::allocate, "allocate" },
			{ ASTNodeType::allocate_list, "allocate_list" },
			{ ASTNodeType::allocate_string, "allocate_string" },
			{ ASTNodeType::allocate_zero, "allocate_zero" },
			{ ASTNodeType::list, "list" },
			{ ASTNodeType::label, "label" },
			{ ASTNodeType::equate, "equate" },
			{ ASTNodeType::global, "global" },
			{ ASTNodeType::global_section, "global_section" },
			{ ASTNodeType::_extern, "extern" },

			{ ASTNodeType::reg_ex, "reg_ex" },

			{ ASTNodeType::reg, "reg" },
			{ ASTNodeType::regid, "regid" },
			{ ASTNodeType::regmode, "regmode" },
			{ ASTNodeType::reg_index, "reg_index" },

			{ ASTNodeType::flagid, "flagid" },

			{ ASTNodeType::ctlid, "ctlid" },
			{ ASTNodeType::cacheid, "cacheid" },

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

			// section: the list of sections. Each section_unit keeps its own node (name as child[0]
			// when given, then its statements, spliced in from the body list) so calculateSection
			// sees one node per section with its name; the list nodes themselves are spliced away.
			{ { NT::section, AT::program, -1 }, { { NT::section_unit, true, false }, { NT::section, true, true } } },
			{ { NT::section, AT::program, -1 }, { { NT::section_unit, true, false } } },

			{ { NT::section_unit, AT::text_section, -1 }, { { TT::text, false, false }, { NT::text_section, true, true } } },
			{ { NT::section_unit, AT::data_section, -1 }, { { TT::data, false, false }, { NT::data_section, true, true } } },
			{ { NT::section_unit, AT::bss_section, -1 }, { { TT::bss, false, false }, { NT::bss_section, true, true } } },
			{ { NT::section_unit, AT::text_section, -1 }, { { TT::text, false, false }, { TT::identifier, true, false }, { NT::text_section, true, true } } },
			{ { NT::section_unit, AT::data_section, -1 }, { { TT::data, false, false }, { TT::identifier, true, false }, { NT::data_section, true, true } } },
			{ { NT::section_unit, AT::bss_section, -1 }, { { TT::bss, false, false }, { TT::identifier, true, false }, { NT::bss_section, true, true } } },
			{ { NT::section_unit, AT::text_section, -1 }, { { TT::text, false, false } } },
			{ { NT::section_unit, AT::data_section, -1 }, { { TT::data, false, false } } },
			{ { NT::section_unit, AT::bss_section, -1 }, { { TT::bss, false, false } } },
			{ { NT::section_unit, AT::text_section, -1 }, { { TT::text, false, false }, { TT::identifier, true, false } } },
			{ { NT::section_unit, AT::data_section, -1 }, { { TT::data, false, false }, { TT::identifier, true, false } } },
			{ { NT::section_unit, AT::bss_section, -1 }, { { TT::bss, false, false }, { TT::identifier, true, false } } },
			{ { NT::section_unit, AT::outer_section, -1 }, { { NT::outer_section, true, true } } },

			// outer_section: global/extern declarations outside .text/.data/.bss (before, between or after them)
			{ { NT::outer_section, AT::outer_section, -1 }, { { NT::global, true, false }, { NT::outer_section, true, true } } },
			{ { NT::outer_section, AT::outer_section, -1 }, { { NT::_extern, true, false }, { NT::outer_section, true, true } } },
			{ { NT::outer_section, AT::outer_section, -1 }, { { NT::global, true, false } } },
			{ { NT::outer_section, AT::outer_section, -1 }, { { NT::_extern, true, false } } },

			{ { NT::data_section, AT::data_section, -1 }, { { NT::allocate, true, false }, { NT::data_section, true, true } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::allocate_list, true, false }, { NT::data_section, true, true } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::allocate_string, true, false }, { NT::data_section, true, true } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::label, true, false }, { NT::data_section, true, true } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::equate, true, false }, { NT::data_section, true, true } } },

			{ { NT::data_section, AT::data_section, -1 }, { { NT::allocate, true, false } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::allocate_list, true, false } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::allocate_string, true, false } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::label, true, false } } },
			{ { NT::data_section, AT::data_section, -1 }, { { NT::equate, true, false } } },

			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::allocate_zero, true, false }, { NT::bss_section, true, true } } },
			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::label, true, false }, { NT::bss_section, true, true } } },
			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::equate, true, false }, { NT::bss_section, true, true } } },

			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::allocate_zero, true, false } } },
			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::label, true, false } } },
			{ { NT::bss_section, AT::bss_section, -1 }, { { NT::equate, true, false } } },

			{ { NT::text_section, AT::text_section, -1 }, { { NT::intruction, true, false }, { NT::text_section, true, true } } },
			{ { NT::text_section, AT::text_section, -1 }, { { NT::label, true, false }, { NT::text_section, true, true } } },
			{ { NT::text_section, AT::text_section, -1 }, { { NT::equate, true, false }, { NT::text_section, true, true } } },

			{ { NT::text_section, AT::text_section, -1 }, { { NT::intruction, true, false } } },
			{ { NT::text_section, AT::text_section, -1 }, { { NT::label, true, false } } },
			{ { NT::text_section, AT::text_section, -1 }, { { NT::equate, true, false } } },

			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::add, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::addc, true, false }, {NT::reg, true, false }, { TT::comma, false, false } ,{ NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::addi, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::sub, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::subc, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::subi, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::bxr, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::bxri, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::bor, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::bori, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::bnd, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::bndi, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::shiftl, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::shiftlc, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::shiftli, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::shiftr, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::shiftrc, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::shiftri, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::ror, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::rori, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::rol, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::roli, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::cmp, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::cmpi, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::test, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::testi, true, false }, {NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },

			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::set, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::sets, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, {NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RR, -1 }, { { TT::mov, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, {NT::reg, true, false } } },

			{ { NT::intruction, AT::instruction_N, -1 }, { { TT::block, true, false } } },

			{ { NT::intruction, AT::instruction_R, -1 }, { { TT::push, true, false }, {NT::reg, true, false } } },
			{ { NT::intruction, AT::instruction_R, -1 }, { { TT::pop, true, false }, {NT::reg, true, false } } },

			{ { NT::intruction, AT::instruction_RVRI, -1 }, { { TT::ld, true, false }, { TT::dot, false, false }, { NT::regid, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RVRI, -1 }, { { TT::st, true, false }, { TT::dot, false, false }, { NT::regid, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },

			{ { NT::intruction, AT::instruction_FVRI, -1 }, { { TT::jmp, true, false }, { TT::dot, false, false }, { NT::flagid, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_FVRI, -1 }, { { TT::ijmp, true, false }, { TT::dot, false, false }, { NT::flagid, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_RI, -1 }, { { TT::call, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate16, true, false } } },
			{ { NT::intruction, AT::instruction_N, -1 }, { { TT::ret, true, false } } },
			{ { NT::intruction, AT::instruction_N, -1 }, { { TT::nop, true, false } } },
			{ { NT::intruction, AT::instruction_N, -1},  { { TT::brk, true, false } } },
			{ { NT::intruction, AT::instruction_N, -1 }, { { TT::wait, true, false } } },
			{ { NT::intruction, AT::instruction_N, -1 }, { { TT::halt, true, false } } },

			{ { NT::intruction, AT::instruction_N, -1}, { { TT::syscall, true, false } } },
			{ { NT::intruction, AT::instruction_N, -1}, { { TT::iret, true, false } } },
			{ { NT::intruction, AT::instruction_RII, -1}, { { TT::intervec, true, false }, { NT::reg, true, false }, { TT::comma, false, false },  { NT::immidate8, true, false }, { TT::comma, false, false }, { NT::immidate8, true, false } } },
			{ { NT::intruction, AT::instruction_IXP, -1}, { { TT::interpri, true, false }, { NT::immidate8, true, false }, { TT::comma, false, false },  { NT::immidate8, true, false }  } },

			{ { NT::intruction, AT::instruction_RXR, -1}, { { TT::ldexp, true, false}, { NT::reg, true, false } , { TT::comma, false, false }, { NT::reg_ex, true, false } } },
			{ { NT::intruction, AT::instruction_RXR, -1}, { { TT::stexp, true, false}, { NT::reg, true, false } , { TT::comma, false, false }, { NT::reg_ex, true, false } } },

			// pctl en/dis, atom len: opcode + one operand in byte 3
			{ { NT::intruction, AT::instruction_CTL, -1}, { { TT::pctl, true, false }, { NT::ctlid, true, false } } },
			{ { NT::intruction, AT::instruction_CTL, -1}, { { TT::atom, true, false }, { NT::immidate8, true, false } } },

			{ { NT::intruction, AT::instruction_RR, -1}, { { TT::getabs, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::reg, true, false } } },

			{ { NT::intruction, AT::instruction_CACHE, -1}, { { TT::flush, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate8, true, false }, { TT::comma, false, false }, { NT::cacheid, true, false } } },
			{ { NT::intruction, AT::instruction_CACHE, -1}, { { TT::inval, true, false }, { NT::reg, true, false }, { TT::comma, false, false }, { NT::immidate8, true, false }, { TT::comma, false, false }, { NT::cacheid, true, false } } },

			{ { NT::reg_ex, AT::reg_ex, -1}, { { TT::istack, true, false } } },
			{ { NT::reg_ex, AT::reg_ex, -1}, { { TT::panicvec, true, false } } },
			{ { NT::reg_ex, AT::reg_ex, -1}, { { TT::cpuid, true, false } } },
			{ { NT::reg_ex, AT::reg_ex, -1}, { { TT::intlevel, true, false } } },
			{ { NT::reg_ex, AT::reg_ex, -1}, { { TT::pagebase, true, false } } },
			{ { NT::reg_ex, AT::reg_ex, -1}, { { TT::plimit, true, false } } },

			{ { NT::ctlid, AT::ctlid, -1 }, { { TT::enable, true, false } } },
			{ { NT::ctlid, AT::ctlid, -1 }, { { TT::disable, true, false } } },

			{ { NT::cacheid, AT::cacheid, -1 }, { { TT::ALL, true, false } } },
			{ { NT::cacheid, AT::cacheid, -1 }, { { TT::L2, true, false } } },
			{ { NT::cacheid, AT::cacheid, -1 }, { { TT::TLB, true, false } } },
			{ { NT::cacheid, AT::cacheid, -1 }, { { TT::L1D, true, false } } },
			{ { NT::cacheid, AT::cacheid, -1 }, { { TT::L1I, true, false } } },
			// combined ids (L1I | L1D): children are the id tokens, OR-ed in evaluateCode
			{ { NT::cacheid, AT::cacheid, -1 }, { { NT::cacheid, true, true }, { TT::verticalbar, false, false }, { TT::ALL, true, false } } },
			{ { NT::cacheid, AT::cacheid, -1 }, { { NT::cacheid, true, true }, { TT::verticalbar, false, false }, { TT::L2, true, false } } },
			{ { NT::cacheid, AT::cacheid, -1 }, { { NT::cacheid, true, true }, { TT::verticalbar, false, false }, { TT::TLB, true, false } } },
			{ { NT::cacheid, AT::cacheid, -1 }, { { NT::cacheid, true, true }, { TT::verticalbar, false, false }, { TT::L1D, true, false } } },
			{ { NT::cacheid, AT::cacheid, -1 }, { { NT::cacheid, true, true }, { TT::verticalbar, false, false }, { TT::L1I, true, false } } },

			{ { NT::reg, AT::reg, 0 }, { { NT::regid, true, false }, { TT::dot, false, false }, { NT::regmode, true, false } } },

			{ { NT::regid, AT::__epsilon, 0 }, { { NT::reg_gen, true, true } } },
			{ { NT::regid, AT::__epsilon, 0 }, { { NT::reg_reg, true, true } } },
			{ { NT::regid, AT::regid, -1 }, { { TT::sbp, true, false } } },
			{ { NT::regid, AT::regid, -1 }, { { TT::zero, true, false } } },
			{ { NT::regid, AT::regid, -1 }, { { TT::one, true, false } } },
			{ { NT::regid, AT::regid, -1 }, { { TT::full, true, false } } },
			{ { NT::regid, AT::regid, -1 }, { { TT::pc, true, false } } },
			{ { NT::regid, AT::regid, -1 }, { { TT::stack, true, false } } },
			{ { NT::regid, AT::regid, -1 }, { { TT::flag, true, false } } },

			{ { NT::reg_gen, AT::reg_index, -1 }, { { TT::gen, true, false }, { NT::index, true, false } } },
			{ { NT::reg_reg, AT::reg_index, -1 }, { { TT::reg, true, false }, { NT::index, true, false } } },

			{ { NT::regmode, AT::regmode, -1 }, { { TT::_32B_, true, false } } },
			{ { NT::regmode, AT::regmode, -1 }, { { TT::_16L_, true, false } } },
			{ { NT::regmode, AT::regmode, -1 }, { { TT::_S16H_, true, false } } },
			{ { NT::regmode, AT::regmode, -1 }, { { TT::_8L_, true, false } } },
			{ { NT::regmode, AT::regmode, -1 }, { { TT::_8H_, true, false } } },
			{ { NT::regmode, AT::regmode, -1 }, { { TT::_S16L_, true, false } } },
			{ { NT::regmode, AT::regmode, -1 }, { { TT::_S8L_, true, false } } },
			{ { NT::regmode, AT::regmode, -1 }, { { TT::_S8H_, true, false } } },

			{ { NT::flagid, AT::flagid, -1 }, { { TT::zero, true, false } } },
			{ { NT::flagid, AT::flagid, -1 }, { { TT::neg, true, false } } },
			{ { NT::flagid, AT::flagid, -1 }, { { TT::pos, true, false } } },
			{ { NT::flagid, AT::flagid, -1 }, { { TT::carry, true, false } } },
			{ { NT::flagid, AT::flagid, -1 }, { { TT::carry4, true, false } } },
			{ { NT::flagid, AT::flagid, -1 }, { { TT::overflow, true, false } } },
			{ { NT::flagid, AT::flagid, -1 }, { { TT::one, true, false } } },
			{ { NT::flagid, AT::flagid, -1 }, { { TT::gen, true, false } } },

			{ { NT::immidate16, AT::immidate16, -1 }, { { NT::expression, true, false } } },
			{ { NT::immidate8, AT::immidate8, -1 }, { { NT::expression, true, false } } },

			{ { NT::label, AT::label, 0 }, { { TT::identifier, false, false }, { TT::colon, false, false } } },
			{ { NT::equate, AT::equate, 0 }, { { TT::identifier, false, false }, { TT::equ, false, false }, { NT::expression, true, false } } },
			
			{ { NT::list, AT::list, -1 }, { { TT::openbrace, false, false }, { NT::list_tail, true, true } } },
			{ { NT::list, AT::list, -1 }, { { TT::openbrace, false, false }, { TT::closebrace, false, false } } },
			{ { NT::list_tail, AT::list, -1 }, { { NT::expression, true, false }, { TT::comma, false, false },  { NT::list_tail, true, true } } },
			{ { NT::list_tail, AT::list, -1 }, { { NT::expression, true, false }, { TT::closebrace, false, false} } },

			{ { NT::global, AT::global, 1}, { { TT::global, false, false }, { TT::identifier, true, false } } },
			// global section <name> / global section (no name = the unnamed sections): sections with that name keep it unmangled
			{ { NT::global, AT::global_section, 2}, { { TT::global, false, false }, { TT::section, false, false }, { TT::identifier, true, false } } },
			{ { NT::global, AT::global_section, -1}, { { TT::global, false, false }, { TT::section, false, false } } },
			{ { NT::_extern, AT::_extern, 1}, { { TT::_extern, false, false}, { TT::identifier, true, false } } },

			{ { NT::allocate, AT::allocate, -1 }, { { NT::expression, true, false }, { TT::comma, false, false },  { NT::expression, true, false } } },
			{ { NT::allocate_list, AT::allocate_list, -1 }, { { NT::expression, true, false }, { TT::openparen, false, false }, { NT::expression, true, false }, { TT::closeparen, false, false }, { TT::comma, false, false }, { NT::list, true, false } } },
			{ { NT::allocate_string, AT::allocate_string, -1 }, { { NT::expression, true, false }, { TT::comma, false, false }, { TT::string, true, false } } },
			{ { NT::allocate_zero, AT::allocate_zero, -1 }, { { NT::expression, true, false } } },

			{ { NT::index, AT::index, -1 }, { { TT::openbrack, false, false }, { NT::expression, true, false }, { TT::closebrack, false, false } } },

			{ { NT::expression, AT::__epsilon, 0 }, { { NT::expressionL13, true, true } } },

			{ { NT::expressionL13, AT::__epsilon, 1 }, { { NT::expressionL12, true, false }, { TT::selector, false, false }, { NT::expressionL13, true, false }, { TT::colon, false, false }, { NT::expressionL13, true, false } } },
			{ { NT::expressionL13, AT::__epsilon, 0 }, { { NT::expressionL12, true, true } } },

			{ { NT::expressionL12, AT::__epsilon, 1 }, { { NT::expressionL12, true, false }, { TT::logverticalbar, false, false }, { NT::expressionL11, true, false } } },
			{ { NT::expressionL12, AT::__epsilon, 0 }, { { NT::expressionL11, true, true } } },

			{ { NT::expressionL11, AT::__epsilon, 1 }, { { NT::expressionL11, true, false }, { TT::logampersend, false, false }, { NT::expressionL10, true, false } } },
			{ { NT::expressionL11, AT::__epsilon, 0 }, { { NT::expressionL10, true, true } } },

			{ { NT::expressionL10, AT::__epsilon, 1 }, { { NT::expressionL10, true, false }, { TT::verticalbar, false, false }, { NT::expressionL9, true, false } } },
			{ { NT::expressionL10, AT::__epsilon, 0 }, { { NT::expressionL9, true, true } } },

			{ { NT::expressionL9, AT::__epsilon, 1 }, { { NT::expressionL9, true, false }, { TT::caret, false, false }, { NT::expressionL8, true, false } } },
			{ { NT::expressionL9, AT::__epsilon, 0 }, { { NT::expressionL8, true, true } } },

			{ { NT::expressionL8, AT::__epsilon, 1 }, { { NT::expressionL8, true, false }, { TT::ampersend, false, false }, { NT::expressionL7, true, false } } },
			{ { NT::expressionL8, AT::__epsilon, 0 }, { { NT::expressionL7, true, true } } },

			{ { NT::expressionL7, AT::__epsilon, 1 }, { { NT::expressionL7, true, false }, { TT::equal, false, false }, { NT::expressionL6, true, false } } },
			{ { NT::expressionL7, AT::__epsilon, 1 }, { { NT::expressionL7, true, false }, { TT::notequal, false, false }, { NT::expressionL6, true, false } } },
			{ { NT::expressionL7, AT::__epsilon, 0 }, { { NT::expressionL6, true, true } } },

			{ { NT::expressionL6, AT::__epsilon, 1 }, { { NT::expressionL6, true, false }, { TT::less, false, false }, { NT::expressionL5, true, false } } },
			{ { NT::expressionL6, AT::__epsilon, 1 }, { { NT::expressionL6, true, false }, { TT::greater, false, false }, { NT::expressionL5, true, false } } },
			{ { NT::expressionL6, AT::__epsilon, 1 }, { { NT::expressionL6, true, false }, { TT::lessequal, false, false }, { NT::expressionL5, true, false } } },
			{ { NT::expressionL6, AT::__epsilon, 1 }, { { NT::expressionL6, true, false }, { TT::greaterequal, false, false }, { NT::expressionL5, true, false } } },
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
			{ { NT::expressionL1, AT::__epsilon, 0 }, { { TT::lognot, false, false }, { NT::expressionL0, true, false } } },
			{ { NT::expressionL1, AT::__epsilon, 0 }, { { TT::plus, false, false }, { NT::expressionL0, true, false } } },
			{ { NT::expressionL1, AT::__epsilon, 0 }, { { TT::minus, false, false }, { NT::expressionL0, true, false } } },
			{ { NT::expressionL1, AT::__epsilon, 0 }, { { NT::expressionL0, true, true } } },

			{ { NT::expressionL0, AT::__epsilon, 1 }, { { TT::openparen, false, false }, { NT::expressionL13, true, true }, { TT::closeparen, false, false } } },
			{ { NT::expressionL0, AT::__epsilon, 0 }, { { NT::number, false , false } } },
			{ { NT::expressionL0, AT::__epsilon, 0 }, { { TT::identifier, false , false } } },
			{ { NT::expressionL0, AT::__epsilon, 0 }, { { TT::curline, false , false } } },

			{ { NT::number, AT::__epsilon, 0 }, { { TT::hexnum, false, false } } },
			{ { NT::number, AT::__epsilon, 0 }, { { TT::decnum, false, false } } },
			{ { NT::number, AT::__epsilon, 0 }, { { TT::octnum, false, false } } },
			{ { NT::number, AT::__epsilon, 0 }, { { TT::binnum, false, false } } },
			{ { NT::number, AT::__epsilon, 0 }, { { TT::character, false, false } } }
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
			using SectionType = format_data::SectionType;
			using OperationType = format_data::OperationType;
			
			enum class ExprType {
				CONSTANT,
				LINEAR,
				COMPLEX
			};
			
			
			typedef struct _ReplaceData {
				u32 index;
				u32 size;
				u32 offset;
			} ReplaceData;

			typedef struct _Section {
				SectionType type;
				std::string name;
				std::vector<u8> data;
				std::unordered_multimap<u32, ReplaceData> operationMap;
			} Section;
			
			typedef struct _OperationToken {
				OperationType type;
				std::string text;
				s32 value;
			} OperationToken;
			
			typedef struct _HashOperation {
				size_t operator()(const std::vector<OperationToken>& k) const {
					size_t h = 0;
					for (auto it = k.cbegin(); it != k.cend(); ++it) {
						size_t h1 = std::hash<OperationType>()(it->type);
						size_t h2 = std::hash<s32>()(it->value) ^ std::hash<std::string>()(it->text);
						size_t h3 = h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
						h <<= 1;
						h += h3;
					}
					return h;
				}
			} HashOperation;

			typedef struct _EqualOperation {
				bool  operator()(const std::vector<OperationToken>& A, const std::vector<OperationToken>& B) const {
					if (A.size() != B.size())
						return false;
					auto ai = A.cbegin();
					auto bi = B.cbegin();
					while (ai != A.cend()) {
						// text matters: IDENTIFIER tokens all carry value 0, so without it every
						// single-label expression would collapse into the first one seen
						if (ai->value != bi->value || ai->type != bi->type || ai->text != bi->text)
							return false;
						++ai;
						++bi;
					}
					return true;
				}
			} EqualOperation;

			typedef struct _ExprData {
				ExprType type;
				s32 value;
				std::unordered_map<size_t, s32> coeff;
				std::vector<OperationToken> operations;
			} ExprData;

			enum class LinearKind {
				NONE,
				ADD_LIKE,
				SUB_LIKE,
				SCALE_LIKE,
				NEG_LIKE
			};

			// EvaluateFunction return codes: 0 = success, anything else is propagated
			// unchanged by Evaluator::evaluate()/evaluator::evaluate() up to assemble().
			enum class EvaluateError {
				SUCCESS = 0,
				MISSING_EXPRESSION,  // a child node's expression value wasn't computed (unresolved identifier / invalid subexpression)
				NEGATIVE_INDEX,      // index[...] evaluated to a negative constant
				NOT_CONSTANT,        // a size/length expression (allocate, allocate_zero) must fold to a constant
				GLOBAL_UNDEFINED,    // `global name` but name is never defined in this file
				EXTERN_DEFINED,      // `extern name` but name is also defined in this file
				INVALID_SIZE,        // negative total size, or list element size <= 0 / not dividing the total
				INITIALIZER_TOO_LONG, // list has more elements / string more bytes than the total size holds
				GLOBAL_SECTION_UNDEFINED  // `global section name` but no section of that name in this file
			};

			inline const char* evaluateErrorName(int code) {
				switch ((EvaluateError)code) {
				case EvaluateError::SUCCESS: return "success";
				case EvaluateError::MISSING_EXPRESSION: return "missing expression";
				case EvaluateError::NEGATIVE_INDEX: return "negative index";
				case EvaluateError::NOT_CONSTANT: return "not constant";
				case EvaluateError::GLOBAL_UNDEFINED: return "global symbol is not defined in this file";
				case EvaluateError::EXTERN_DEFINED: return "extern symbol is defined in this file";
				case EvaluateError::INVALID_SIZE: return "invalid size";
				case EvaluateError::INITIALIZER_TOO_LONG: return "initializer too long";
				case EvaluateError::GLOBAL_SECTION_UNDEFINED: return "global section is not defined in this file";
				}
				return "unknown error";
			}

			std::unordered_map<size_t, s32> addCoeff(const std::unordered_map<size_t, s32>& a, const std::unordered_map<size_t, s32>& b) {
				std::unordered_map<size_t, s32> result = a;
				for (auto it = b.cbegin(); it != b.cend(); ++it) {
					s32 v = result[it->first] + it->second;
					if (v == 0) result.erase(it->first);
					else result[it->first] = v;
				}
				return result;
			}
			
			std::unordered_map<size_t, s32> subCoeff(const std::unordered_map<size_t, s32>& a, const std::unordered_map<size_t, s32>& b) {
				std::unordered_map<size_t, s32> result = a;
				for (auto it = b.cbegin(); it != b.cend(); ++it) {
					s32 v = result[it->first] - it->second;
					if (v == 0) result.erase(it->first);
					else result[it->first] = v;
				}
				return result;
			}

			std::unordered_map<size_t, s32> scaleCoeff(const std::unordered_map<size_t, s32>& a, s32 factor) {
				std::unordered_map<size_t, s32> result;
				for (auto it = a.cbegin(); it != a.cend(); ++it) {
					s32 v = it->second * factor;
					if (v != 0) result[it->first] = v;
				}
				return result;
			}

			typedef struct _InstValue {
				u32 data;
				std::vector<ReplaceData> replace;

				_InstValue(void) : data(0) {}
			} InstValue;

			typedef struct _DataValue {
				u32 length;
				u32 data;
				std::vector<ReplaceData> replace;
				
				_DataValue(void) : length(0), data(0) {}
			} DataValue;

			typedef struct _EvalData {
				std::string fileName; // set before evaluation; read by format::formatData() (via mangleLabel) when building the serialized Format, not used during evaluation itself -- idenifierValue/expressionValue stay keyed by the raw label name throughout evaluation

				std::unordered_set<std::string> globalIdentifier;
				std::unordered_set<std::string> externIdentifier;
				std::unordered_set<std::string> globalSectionIdentifier;

				std::unordered_map<std::string, ExprData> idenifierValue;
				std::unordered_map<AST*, ExprData> expressionValue;
				std::unordered_map<AST*, InstValue> instructionValue;
				std::unordered_map<AST*, DataValue> dataValue;

				std::vector<AST*> sectioningArr;
				std::unordered_map<AST*, size_t> sectionIdMap;
				std::vector<u32> sectionByte;

				std::vector<Section> sections;
				std::vector<std::vector<OperationToken>> operationVector;
				std::vector<u32> compressionIndex;

				int curLineCount;

				_EvalData() : curLineCount(0) {}
			} EvalData;

			// Labels are mangled with the (output-binary-based) file name so that identically
			// named labels defined in different object files don't collide once the linker merges
			// their symbol tables (see linker::evaluate::functions::mapExpr's cross-file lookup).
			// '#' can't appear in a source identifier, so this can never collide with a real name.
			// Empty fileName leaves the label unmangled (e.g. for ad-hoc/debug evaluation). Applied
			// only in format::formatData(), not during evaluation -- calculateConst's identifier
			// lookups stay on the raw label name, since idenifierValue is keyed by it directly.
			inline std::string mangleLabel(const std::string& fileName, const std::string& label) {
				return fileName.empty() ? label : (fileName + "#" + label);
			}

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

			int char2int(std::string text) {
				int ans = 0;

				auto it = text.begin();
				if (it != text.end()) ++it;

				if (*it == '\\') {
					++it;

					switch (*it) {
					case 'n': ans = '\n'; break;
					case 't': ans = '\t'; break;
					case '\\': ans = '\\'; break;
					case '\'': ans = '\''; break;
					case '\"': ans = '\"'; break;
					default:
						for (it; *it != '\''; ++it) {
							ans *= 10;
							ans += (*it - '0');
						}
					}
				}
				else {
					ans = *it;
				}

				return ans;
			}

			// "..." string literal -> bytes. Escapes match char2int: \n \t \\ \' \" and decimal \65;
			// also \r. Any other escaped character stands for itself.
			std::vector<u8> string2bytes(const std::string& text) {
				std::vector<u8> out;
				size_t end = (text.size() >= 2) ? text.size() - 1 : text.size();
				for (size_t i = 1; i < end; i++) {
					if (text[i] != '\\' || i + 1 >= end) {
						out.push_back((u8)text[i]);
						continue;
					}
					char c = text[++i];
					switch (c) {
					case 'n': out.push_back('\n'); break;
					case 't': out.push_back('\t'); break;
					case 'r': out.push_back('\r'); break;
					default:
						if (c >= '0' && c <= '9') {
							int v = 0;
							for (int n = 0; n < 3 && i < end && text[i] >= '0' && text[i] <= '9'; n++, i++)
								v = v * 10 + (text[i] - '0');
							i--;
							out.push_back((u8)v);
						}
						else
							out.push_back((u8)c);
					}
				}
				return out;
			}

			int pow(int base, u32 power) {
				int ans = 1;
				for (u32 i = 0; i < power; i++)
					ans *= base;
				return ans;
			}

			std::map<TokenType, u32> instructionBase = {
				{ TokenType::add, 0x0 },
				{ TokenType::addc, 0x1 },
				{ TokenType::addi, 0x2},
				{ TokenType::sub, 0x3 },
				{ TokenType::subc, 0x4 },
				{ TokenType::subi, 0x5 },
				{ TokenType::bxr, 0x6 },
				{ TokenType::bxri, 0x7 },
				{ TokenType::bor, 0x8 },
				{ TokenType::bori, 0x9 },
				{ TokenType::bnd, 0xA },
				{ TokenType::bndi, 0xB },
				{ TokenType::rol, 0xC },
				{ TokenType::roli, 0xD },
				{ TokenType::ror, 0xE },
				{ TokenType::rori, 0xF },
				{ TokenType::shiftl, 0x10 },
				{ TokenType::shiftlc, 0x11 },
				{ TokenType::shiftli, 0x12 },
				{ TokenType::shiftr, 0x13 },
				{ TokenType::shiftrc, 0x14 },
				{ TokenType::shiftri, 0x15 },
				{ TokenType::cmp, 0x16 },
				{ TokenType::cmpi, 0x17 },
				{ TokenType::test, 0x18 },
				{ TokenType::testi, 0x19 },

				{ TokenType::set, 0x1A },
				{ TokenType::sets, 0x1B },
				{ TokenType::mov, 0x1C },

				{ TokenType::block, 0x1D },

				{ TokenType::pop, 0x1E },
				{ TokenType::push, 0x1F },

				{ TokenType::ld, 0x20 },
				{ TokenType::st, 0x40 },

				{ TokenType::jmp, 0x60 },
				{ TokenType::ijmp, 0x68 },
				{ TokenType::call, 0x70 },
				{ TokenType::ret, 0x71 },
				{ TokenType::nop, 0x72 },
				{ TokenType::brk, 0x73 },
				{ TokenType::wait, 0x74 },
				{ TokenType::halt, 0x75 },

				{ TokenType::syscall, 0x76 },
				{ TokenType::iret, 0x77 },
				{ TokenType::intervec, 0x78 },
				{ TokenType::interpri, 0x79 },

				{ TokenType::ldexp, 0x7A },
				{ TokenType::stexp, 0x7B },

				{ TokenType::pctl, 0x7C },
				{ TokenType::getabs, 0x7D },
				{ TokenType::flush, 0x7E },	// cache, id md bit set
				{ TokenType::inval, 0x7E },	// cache, id md bit clear
				{ TokenType::atom, 0x7F }
			};

			std::map<TokenType, u32> regExBase = {
				{ TokenType::cpuid, 0x0 },
				{ TokenType::panicvec, 0x1 },
				{ TokenType::istack, 0x2 },
				{ TokenType::intlevel, 0x3 },
				{ TokenType::pagebase, 0x4 },
				{ TokenType::plimit, 0x5 }
			};

			std::map<TokenType, u32> regBase = {
				{ TokenType::reg, 0x0 },
				{ TokenType::gen, 0x0 },
				{ TokenType::sbp, 0x19 },
				{ TokenType::zero, 0x1A },
				{ TokenType::one, 0x1B },
				{ TokenType::full, 0x1C },
				{ TokenType::pc, 0x1D },
				{ TokenType::stack, 0x1E },
				{ TokenType::flag, 0x1F },

				{ TokenType::_32B_, 0 },
				{ TokenType::_16L_, 1 },
				{ TokenType::_8L_, 2 },
				{ TokenType::_8H_, 3 },
				{ TokenType::_S16H_, 4 },
				{ TokenType::_S16L_, 5 },
				{ TokenType::_S8L_, 6 },
				{ TokenType::_S8H_, 7 }
			};

			std::map<TokenType, u32> flagBase = {
				{ TokenType::zero, 0 },
				{ TokenType::neg, 1 },
				{ TokenType::pos, 2 },
				{ TokenType::carry, 3 },
				{ TokenType::carry4, 4 },
				{ TokenType::overflow, 5 },
				{ TokenType::one, 6 },
				{ TokenType::gen, 7 },
			};

			std::map<TokenType, u32> ctlBase = {
				{ TokenType::disable, 0 },
				{ TokenType::enable, 1 }
			};

			std::map<TokenType, u32> cacheBase = {
				{ TokenType::L1I, 0x1 },
				{ TokenType::L1D, 0x2 },
				{ TokenType::TLB, 0x4 },
				{ TokenType::L2, 0x8 },
				{ TokenType::ALL, 0x40 }
			};

			// md bit of the cache id byte: set = flush, clear = inval
			const u32 cacheFlushBit = 0x80;

			int calculateSection(AST* cur, void* vpData) {
				EvalData* data = (EvalData*)vpData;

				SectionType type = SectionType::OUTER;
				switch (cur->type) {
					case (u64)ASTNodeType::text_section + (u64)TokenType::__end: type = SectionType::TEXT; goto process_section;
					case (u64)ASTNodeType::data_section + (u64)TokenType::__end: type = SectionType::DATA; goto process_section;
					case (u64)ASTNodeType::bss_section + (u64)TokenType::__end: type = SectionType::BSS; goto process_section;
					case (u64)ASTNodeType::outer_section + (u64)TokenType::__end: type = SectionType::OUTER; goto process_section;
						process_section: {
							size_t index = data->sections.size();
							for (auto it = data->sectioningArr.cbegin(); it != data->sectioningArr.cend(); ++it)
								data->sectionIdMap.insert({*it, index});

							Section nSection;
							nSection.type = type;
							nSection.name = (!cur->child.empty() && cur->child[0]->type == (u64)TokenType::identifier) ? cur->child[0]->text : "";
							data->sections.push_back(nSection);
							data->sectionByte.push_back(0);
							break;
						}

					case (u64)ASTNodeType::instruction_N + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_R + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_RI + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_RR + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_II + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_RII + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_IRI + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_RVRI + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_FVRI + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_IXP + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_RXR + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_CTL + (u64)TokenType::__end:
					case (u64)ASTNodeType::instruction_CACHE + (u64)TokenType::__end:
					case (u64)ASTNodeType::label + (u64)TokenType::__end:
					case (u64)ASTNodeType::allocate + (u64)TokenType::__end:
					case (u64)ASTNodeType::allocate_list + (u64)TokenType::__end:
					case (u64)ASTNodeType::allocate_string + (u64)TokenType::__end:
					case (u64)ASTNodeType::allocate_zero + (u64)TokenType::__end:
					case (u64)TokenType::curline:
						data->sectioningArr.push_back(cur);
						break;
				}

				return (int)EvaluateError::SUCCESS;
			}

			ExprData combineBinary(const ExprData& a, const ExprData& b, OperationType opType, const std::string& opText, s32(*constFn)(s32, s32), LinearKind kind) {
				ExprData r;
				r.operations = a.operations;
				r.operations.insert(r.operations.end(), b.operations.begin(), b.operations.end());
				r.operations.push_back({ opType, opText });

				if (a.type == ExprType::CONSTANT && b.type == ExprType::CONSTANT) {
					r.type = ExprType::CONSTANT;
					r.value = constFn(a.value, b.value);
					return r;
				}

				bool linearOk = false;
				if (kind == LinearKind::ADD_LIKE) {
					if (a.type == ExprType::LINEAR && b.type == ExprType::CONSTANT) {
						r.coeff = a.coeff; 
						r.value = a.value + b.value; 
						linearOk = true;
					}
					else if (a.type == ExprType::CONSTANT && b.type == ExprType::LINEAR) {
						r.coeff = b.coeff; 
						r.value = a.value + b.value; 
						linearOk = true;
					}
					else if (a.type == ExprType::LINEAR && b.type == ExprType::LINEAR) {
						r.coeff = addCoeff(a.coeff, b.coeff); 
						r.value = a.value + b.value; 
						linearOk = true;
					}
				}
				else if (kind == LinearKind::SUB_LIKE) {
					if (a.type == ExprType::LINEAR && b.type == ExprType::CONSTANT) {
						r.coeff = a.coeff; 
						r.value = a.value - b.value; 
						linearOk = true;
					}
					else if (a.type == ExprType::CONSTANT && b.type == ExprType::LINEAR) {
						r.coeff = scaleCoeff(b.coeff, -1);
						r.value = a.value - b.value;
						linearOk = true;
					}
					else if (a.type == ExprType::LINEAR && b.type == ExprType::LINEAR) {
						r.coeff = subCoeff(a.coeff, b.coeff); 
						r.value = a.value - b.value; 
						linearOk = true;
					}
				}
				else if (kind == LinearKind::SCALE_LIKE) {
					if (a.type == ExprType::LINEAR && b.type == ExprType::CONSTANT) {
						r.coeff = scaleCoeff(a.coeff, b.value); 
						r.value = a.value * b.value; 
						linearOk = true;
					}
					else if (a.type == ExprType::CONSTANT && b.type == ExprType::LINEAR) {
						r.coeff = scaleCoeff(b.coeff, a.value); 
						r.value = a.value * b.value; 
						linearOk = true;
					}
				}

				if (linearOk)
					r.type = r.coeff.empty() ? ExprType::CONSTANT : ExprType::LINEAR;
				else {
					r.type = ExprType::COMPLEX;
					r.value = 0;
				}
				return r;
			}

			ExprData combineUnary(const ExprData& a, OperationType opType, const std::string& opText, s32(*constFn)(s32), LinearKind kind) {
				ExprData r;
				r.operations = a.operations;
				r.operations.push_back({ opType, opText });

				if (a.type == ExprType::CONSTANT) {
					r.type = ExprType::CONSTANT;
					r.value = constFn(a.value);
					return r;
				}

				bool linearOk = false;
				if (kind == LinearKind::NEG_LIKE) {
					if (a.type == ExprType::LINEAR) {
						r.coeff = scaleCoeff(a.coeff, -1);
						r.value = -a.value;
						linearOk = true;
					}
				}

				if (linearOk)
					r.type = ExprType::LINEAR;
				else {
					r.type = ExprType::COMPLEX;
					r.value = 0;
				}
				return r;
			}

			// Expression nodes only (operators, literals, identifiers, index). A node whose operands
			// aren't resolved yet (forward-referenced label) is skipped, not an error: calculateAddr
			// runs before every label is defined, and evaluateCode retries it once they all are.
			// Whoever consumes the value (immidate16/allocate/...) reports MISSING_EXPRESSION.
			int calculateExpr(AST* cur, EvalData* data) {
				bool child1exp = (cur->child.size() >= 1) ? data->expressionValue.find(cur->child[0]) != data->expressionValue.end() : false;
				bool child2exp = (cur->child.size() >= 2) ? data->expressionValue.find(cur->child[1]) != data->expressionValue.end() : false;
				bool child3exp = (cur->child.size() >= 3) ? data->expressionValue.find(cur->child[2]) != data->expressionValue.end() : false;
				
				OperationType operationType;
				std::string operationText;
				s32(*function)(s32, s32);
				s32(*unaryFunction)(s32);
				LinearKind linearKind;				

				switch (cur->type) {
				case (u64)TokenType::verticalbar: operationType = OperationType::BOR; operationText = "|"; function = [](s32 a, s32 b) { return a | b; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::caret: operationType = OperationType::BXR; operationText = "^"; function = [](s32 a, s32 b) { return a ^ b; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::ampersend: operationType = OperationType::BND; operationText = "&"; function = [](s32 a, s32 b) { return a & b; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::leftshift: operationType = OperationType::SHIFTL; operationText = "<<"; function = [](s32 a, s32 b) { return a << b; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::rightshift: operationType = OperationType::SHIFTR; operationText = ">>"; function = [](s32 a, s32 b) { return a >> b; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::plus:
					if (cur->child.size() == 1) {
						if (child1exp) data->expressionValue[cur] = data->expressionValue[cur->child[0]];
						break;
					}
					operationType = OperationType::ADD; operationText = "+"; function = [](s32 a, s32 b) { return a + b; }; linearKind = LinearKind::ADD_LIKE; goto binaryOp;
				case (u64)TokenType::minus:
					if (cur->child.size() == 1) { operationType = OperationType::NEG; operationText = "-"; unaryFunction = [](s32 a) { return -a; }; linearKind = LinearKind::NEG_LIKE; goto unaryOp; }
					operationType = OperationType::SUB; operationText = "-"; function = [](s32 a, s32 b) { return a - b; }; linearKind = LinearKind::SUB_LIKE; goto binaryOp;
				case (u64)TokenType::star: operationType = OperationType::MULT; operationText = "*"; function = [](s32 a, s32 b) { return a * b; }; linearKind = LinearKind::SCALE_LIKE; goto binaryOp;
				case (u64)TokenType::slash: operationType = OperationType::DIV; operationText = "/"; function = [](s32 a, s32 b) { return a / b; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::percent: operationType = OperationType::MOD; operationText = "%"; function = [](s32 a, s32 b) { return a % b; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::dstar: operationType = OperationType::POW; operationText = "**"; function = [](s32 a, s32 b) -> s32 { return pow(a, (u32)b); }; linearKind = LinearKind::NONE; goto binaryOp;

				case (u64)TokenType::less: operationType = OperationType::LESS; operationText = "<"; function = [](s32 a, s32 b) -> s32 { return a < b ? 1 : 0; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::greater: operationType = OperationType::GREATER; operationText = ">"; function = [](s32 a, s32 b) -> s32 { return a > b ? 1 : 0; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::lessequal: operationType = OperationType::LESSEQUAL; operationText = "<="; function = [](s32 a, s32 b) -> s32 { return a <= b ? 1 : 0; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::greaterequal: operationType = OperationType::GREATEQUAL; operationText = ">="; function = [](s32 a, s32 b) -> s32 { return a >= b ? 1 : 0; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::equal: operationType = OperationType::EQUAL; operationText = "=="; function = [](s32 a, s32 b) -> s32 { return a == b ? 1 : 0; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::notequal: operationType = OperationType::NOTEQUAL; operationText = "!="; function = [](s32 a, s32 b) -> s32 { return a != b ? 1 : 0; }; linearKind = LinearKind::NONE; goto binaryOp;

				case (u64)TokenType::logampersend: operationType = OperationType::LAND; operationText = "&&"; function = [](s32 a, s32 b) -> s32 { return (a != 0 && b != 0) ? 1 : 0; }; linearKind = LinearKind::NONE; goto binaryOp;
				case (u64)TokenType::logverticalbar: operationType = OperationType::LOR; operationText = "||"; function = [](s32 a, s32 b) -> s32 { return (a != 0 || b != 0) ? 1 : 0; }; linearKind = LinearKind::NONE; goto binaryOp;

				binaryOp:
					if (child1exp && child2exp)
						data->expressionValue[cur] = combineBinary(data->expressionValue[cur->child[0]], data->expressionValue[cur->child[1]], operationType, operationText, function, linearKind);
					break;

				case (u64)TokenType::tilde: operationType = OperationType::BNT; operationText = "~"; unaryFunction = [](s32 a) { return ~a; }; linearKind = LinearKind::NONE; goto unaryOp;
				case (u64)TokenType::lognot: operationType = OperationType::LNOT; operationText = "!"; unaryFunction = [](s32 a) -> s32 { return a == 0 ? 1 : 0; }; linearKind = LinearKind::NONE; goto unaryOp;

				unaryOp:
					if (child1exp)
						data->expressionValue[cur] = combineUnary(data->expressionValue[cur->child[0]], operationType, operationText, unaryFunction, linearKind);
					break;

				case (u64)TokenType::selector: {
					if (child1exp && child2exp && child3exp) {
						ExprData& condV = data->expressionValue[cur->child[0]];
						ExprData& thenV = data->expressionValue[cur->child[1]];
						ExprData& elseV = data->expressionValue[cur->child[2]];

						if (condV.type == ExprType::CONSTANT) {
							data->expressionValue[cur] = (condV.value != 0) ? thenV : elseV;
						}
						else {
							ExprData r;
							r.type = ExprType::COMPLEX;
							r.value = 0;
							r.operations = condV.operations;
							r.operations.insert(r.operations.end(), thenV.operations.begin(), thenV.operations.end());
							r.operations.insert(r.operations.end(), elseV.operations.begin(), elseV.operations.end());
							r.operations.push_back({ OperationType::SELECT, "?:" });
							data->expressionValue[cur] = r;
						}
					}
					break;
				}

				case (u64)TokenType::hexnum: { s32 v = hex2int(cur->text); data->expressionValue[cur] = { ExprType::CONSTANT, v, {}, { { OperationType::CONSTANT, cur->text, v } } }; break; }
				case (u64)TokenType::decnum: { s32 v = dec2int(cur->text); data->expressionValue[cur] = { ExprType::CONSTANT, v, {}, { { OperationType::CONSTANT, cur->text, v } } }; break; }
				case (u64)TokenType::octnum: { s32 v = oct2int(cur->text); data->expressionValue[cur] = { ExprType::CONSTANT, v, {}, { { OperationType::CONSTANT, cur->text, v } } }; break; }
				case (u64)TokenType::binnum: { s32 v = bin2int(cur->text); data->expressionValue[cur] = { ExprType::CONSTANT, v, {}, { { OperationType::CONSTANT, cur->text, v } } }; break; }
				case (u64)TokenType::character: { s32 v = char2int(cur->text); data->expressionValue[cur] = { ExprType::CONSTANT, v, {}, { { OperationType::CONSTANT, cur->text, v } } }; break; }

				case (u64)TokenType::identifier: {
					auto find = data->idenifierValue.find(cur->text);
					if (find != data->idenifierValue.end())
						data->expressionValue[cur] = find->second;
					// defined in another file: value unknown until link, so always relocated
					else if (data->externIdentifier.find(cur->text) != data->externIdentifier.end())
						data->expressionValue[cur] = { ExprType::COMPLEX, 0, {}, { { OperationType::IDENTIFIER, cur->text } } };
					break;
				}

				case (u64)ASTNodeType::index + (u64)TokenType::__end:
					if (child1exp) {
						if (data->expressionValue[cur->child[0]].value < 0)
							return (int)EvaluateError::NEGATIVE_INDEX;
						data->expressionValue[cur] = data->expressionValue[cur->child[0]];
					}
					break;
				}

				return (int)EvaluateError::SUCCESS;
			}

			int calculateAddr(AST* cur, void* vpData) {
				EvalData* data = (EvalData*)vpData;

				int code = calculateExpr(cur, data);
				if (code) return code;

				switch (cur->type) {
				case (u64)ASTNodeType::global + (u64)TokenType::__end:
					data->globalIdentifier.insert(cur->text);
					break;
				case (u64)ASTNodeType::global_section + (u64)TokenType::__end:
					// no identifier -> "" (the unnamed sections)
					data->globalSectionIdentifier.insert(cur->child.empty() ? "" : cur->text);
					break;
				case (u64)ASTNodeType::_extern + (u64)TokenType::__end:
					data->externIdentifier.insert(cur->text);
					break;
					
				case (u64)ASTNodeType::instruction_N + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_R + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RR + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_II + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RII + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_IRI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RVRI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_FVRI + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_IXP + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_RXR + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_CTL + (u64)TokenType::__end:
				case (u64)ASTNodeType::instruction_CACHE + (u64)TokenType::__end: {
					size_t section_id = data->sectionIdMap[cur];
					data->sectionByte[section_id] += 4;
					break;
				}

				case (u64)ASTNodeType::label + (u64)TokenType::__end: {
					size_t section_id = data->sectionIdMap[cur];

					ExprData nData;
					nData.type = ExprType::LINEAR;
					nData.value = data->sectionByte[section_id];
					nData.coeff[section_id] = 1;
					nData.operations.push_back({ OperationType::IDENTIFIER, cur->text });

					data->idenifierValue[cur->text] = nData;
					break;
				}

				// $: address of the current instruction/data (section byte count before it).
				// Each $ gets a hidden label "#curline#N" ('#' can't appear in a source identifier)
				// so a relocated use (jmp $) has a symbol the linker can resolve.
				case (u64)TokenType::curline: {
					size_t section_id = data->sectionIdMap[cur];

					std::string name = "#curline#" + std::to_string(data->curLineCount);
					data->curLineCount++;

					ExprData nData;
					nData.type = ExprType::LINEAR;
					nData.value = data->sectionByte[section_id];
					nData.coeff[section_id] = 1;
					nData.operations.push_back({ OperationType::IDENTIFIER, name });

					data->idenifierValue[name] = nData;
					data->expressionValue[cur] = nData;
					break;
				}

				// name equ expr: the name takes a copy of the expression's value. If it can't be
				// computed yet (forward-referenced label), evaluateCode retries it.
				case (u64)ASTNodeType::equate + (u64)TokenType::__end: {
					auto find = data->expressionValue.find(cur->child[0]);
					if (find != data->expressionValue.end())
						data->idenifierValue[cur->text] = find->second;
					break;
				}

				case (u64)ASTNodeType::allocate + (u64)TokenType::__end: {
					size_t section_id = data->sectionIdMap[cur];

					auto find = data->expressionValue.find(cur->child[0]);
					if (find == data->expressionValue.end())
						return (int)EvaluateError::MISSING_EXPRESSION;
					if (find->second.type != ExprType::CONSTANT)
						return (int)EvaluateError::NOT_CONSTANT;

					data->sectionByte[section_id] += find->second.value;
					break;
				}

				// total(element), { e0, e1, ... }: sizes fixed here, elements may be relocated later
				case (u64)ASTNodeType::allocate_list + (u64)TokenType::__end: {
					size_t section_id = data->sectionIdMap[cur];

					auto total = data->expressionValue.find(cur->child[0]);
					auto element = data->expressionValue.find(cur->child[1]);
					if (total == data->expressionValue.end() || element == data->expressionValue.end())
						return (int)EvaluateError::MISSING_EXPRESSION;
					if (total->second.type != ExprType::CONSTANT || element->second.type != ExprType::CONSTANT)
						return (int)EvaluateError::NOT_CONSTANT;

					s32 t = total->second.value, e = element->second.value;
					if (t < 0 || e <= 0 || t % e)
						return (int)EvaluateError::INVALID_SIZE;
					if ((s32)cur->child[2]->child.size() > t / e)
						return (int)EvaluateError::INITIALIZER_TOO_LONG;

					data->sectionByte[section_id] += t;
					break;
				}

				// total, "string": bytes of the string, zero-padded to total
				case (u64)ASTNodeType::allocate_string + (u64)TokenType::__end: {
					size_t section_id = data->sectionIdMap[cur];

					auto total = data->expressionValue.find(cur->child[0]);
					if (total == data->expressionValue.end())
						return (int)EvaluateError::MISSING_EXPRESSION;
					if (total->second.type != ExprType::CONSTANT)
						return (int)EvaluateError::NOT_CONSTANT;

					s32 t = total->second.value;
					if (t < 0)
						return (int)EvaluateError::INVALID_SIZE;
					if ((s32)string2bytes(cur->child[1]->text).size() > t)
						return (int)EvaluateError::INITIALIZER_TOO_LONG;

					data->sectionByte[section_id] += t;
					break;
				}

				case (u64)ASTNodeType::allocate_zero + (u64)TokenType::__end: {
					size_t section_id = data->sectionIdMap[cur];

					auto find = data->expressionValue.find(cur->child[0]);
					if (find == data->expressionValue.end())
						return (int)EvaluateError::MISSING_EXPRESSION;
					if (find->second.type != ExprType::CONSTANT)
						return (int)EvaluateError::NOT_CONSTANT;

					data->sectionByte[section_id] += find->second.value;
					break;
				}
				}

				return (int)EvaluateError::SUCCESS;
			}

			void pushByteN(std::vector<u8>& list, u32 data, int size);

			void push32(std::vector<u8>& list, u32 inst) {
				pushByteN(list, inst, 4);
			}

			// big-endian: the low `size` bytes MSB first; past 4 bytes the value repeats so that
			// it ends on a whole copy (5, 0x11223344 -> 44 11 22 33 44). little-endian is the exact
			// byte reverse of that (44 33 22 11 44).
			void pushByteN(std::vector<u8>& list, u32 data, int size) {
				size_t start = list.size();
				for (int i = size - 1; i >= 0; i--)
					list.push_back((u8)(data >> ((i % 4) * 8)));
				if (!bigEndian)
					std::reverse(list.begin() + start, list.end());
			}

			void appendReplace(std::vector<ReplaceData>& dest, const std::vector<ReplaceData>& src, u32 shift) {
				for (ReplaceData rep : src) {
					rep.offset += shift;
					dest.push_back(rep);
				}
			}

			// key = the byte holding bit `offset` of the field: counted back from the last byte
			// (big-endian) or forward from the first (little-endian) of the `byte`-wide value
			void registerReplace(EvalData* data, u32 sectionId, const std::vector<ReplaceData>& replace, u32 byte) {
				u32 start = (u32)data->sections[sectionId].data.size();
				for (auto it = replace.cbegin(); it != replace.cend(); ++it) {
					u32 pos = bigEndian ? start + byte - 1 - it->offset / 8 : start + it->offset / 8;
					data->sections[sectionId].operationMap.insert({ pos, ReplaceData{it->index, it->size, it->offset % 8} });
				}
			}

			int evaluateCode(AST* cur, void* vpData) {
				EvalData* data = (EvalData*)vpData;

				// every label is in idenifierValue by now, so retry expressions calculateAddr had
				// to skip; children are visited first, so a parent sees its retried operands.
				if (data->expressionValue.find(cur) == data->expressionValue.end()) {
					int code = calculateExpr(cur, data);
					if (code) return code;
				}

				u32 sectionId = data->sectionIdMap[cur];

				switch (cur->type) {
				// calculateAddr has defined every label/equ by now
				case (u64)ASTNodeType::global + (u64)TokenType::__end:
					if (data->idenifierValue.find(cur->text) == data->idenifierValue.end())
						return (int)EvaluateError::GLOBAL_UNDEFINED;
					break;
				case (u64)ASTNodeType::global_section + (u64)TokenType::__end: {
					std::string name = cur->child.empty() ? "" : cur->text;
					bool found = false;
					for (auto it = data->sections.cbegin(); it != data->sections.cend() && !found; ++it)
						found = it->type != SectionType::OUTER && it->name == name;
					if (!found)
						return (int)EvaluateError::GLOBAL_SECTION_UNDEFINED;
					break;
				}
				case (u64)ASTNodeType::_extern + (u64)TokenType::__end:
					if (data->idenifierValue.find(cur->text) != data->idenifierValue.end())
						return (int)EvaluateError::EXTERN_DEFINED;
					break;

				case (u64)ASTNodeType::equate + (u64)TokenType::__end: {
					if (data->idenifierValue.find(cur->text) != data->idenifierValue.end())
						break;
					auto find = data->expressionValue.find(cur->child[0]);
					if (find == data->expressionValue.end())
						return (int)EvaluateError::MISSING_EXPRESSION;
					data->idenifierValue[cur->text] = find->second;
					break;
				}

				case (u64)ASTNodeType::allocate + (u64)TokenType::__end: {
					if (data->expressionValue.find(cur->child[1]) == data->expressionValue.end())
						return (int)EvaluateError::MISSING_EXPRESSION;

					// size in bytes; calculateAddr already required it to be CONSTANT
					data->dataValue[cur].length = (u32)data->expressionValue[cur->child[0]].value;

					ExprData& expr = data->expressionValue[cur->child[1]];
					if (expr.type == ExprType::CONSTANT)
						data->dataValue[cur].data = expr.value;
					else {
						ReplaceData rep = {};
						rep.index = data->operationVector.size();
						rep.offset = 0;
						// never wider than the allocation itself, or the patch spills into the previous data
						rep.size = (data->dataValue[cur].length < 4) ? data->dataValue[cur].length * 8 : 32;
						data->dataValue[cur].data = 0;
						data->dataValue[cur].replace.push_back(rep);
						data->operationVector.push_back(expr.operations);
					}

					registerReplace(data, sectionId, data->dataValue[cur].replace, data->dataValue[cur].length);
					pushByteN(data->sections[sectionId].data, data->dataValue[cur].data, data->dataValue[cur].length);
					break;
				}

				// each element is laid out like a single allocate of the element size; elements
				// past the end of the list are zero. calculateAddr already checked the sizes.
				case (u64)ASTNodeType::allocate_list + (u64)TokenType::__end: {
					u32 total = (u32)data->expressionValue[cur->child[0]].value;
					u32 element = (u32)data->expressionValue[cur->child[1]].value;
					const std::vector<AST*>& elements = cur->child[2]->child;

					for (u32 i = 0; i < total / element; i++) {
						if (i >= elements.size()) {
							pushByteN(data->sections[sectionId].data, 0, element);
							continue;
						}

						auto find = data->expressionValue.find(elements[i]);
						if (find == data->expressionValue.end())
							return (int)EvaluateError::MISSING_EXPRESSION;

						std::vector<ReplaceData> replace;
						u32 value = 0;
						if (find->second.type == ExprType::CONSTANT)
							value = find->second.value;
						else {
							ReplaceData rep = {};
							rep.index = data->operationVector.size();
							rep.offset = 0;
							rep.size = (element < 4) ? element * 8 : 32;
							replace.push_back(rep);
							data->operationVector.push_back(find->second.operations);
						}

						registerReplace(data, sectionId, replace, element);
						pushByteN(data->sections[sectionId].data, value, element);
					}
					break;
				}

				case (u64)ASTNodeType::allocate_string + (u64)TokenType::__end: {
					u32 total = (u32)data->expressionValue[cur->child[0]].value;
					std::vector<u8> bytes = string2bytes(cur->child[1]->text);
					bytes.resize(total, 0);
					data->sections[sectionId].data.insert(data->sections[sectionId].data.end(), bytes.begin(), bytes.end());
					break;
				}

				case (u64)ASTNodeType::instruction_N + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_R + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + (data->instructionValue[cur->child[1]].data << 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 16);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_RI + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + (data->instructionValue[cur->child[1]].data << 16) + (data->instructionValue[cur->child[2]].data);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 0);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_RR + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + (data->instructionValue[cur->child[1]].data << 16) + (data->instructionValue[cur->child[2]].data << 8);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 8);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_II + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + (data->instructionValue[cur->child[1]].data << 16) + (data->instructionValue[cur->child[2]].data);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 0);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_RII + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + (data->instructionValue[cur->child[1]].data << 16) + (data->instructionValue[cur->child[2]].data << 8) + (data->instructionValue[cur->child[3]].data);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 8);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[3]].replace, 0);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_IRI + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + (data->instructionValue[cur->child[1]].data << 16) + (data->instructionValue[cur->child[2]].data << 8) + (data->instructionValue[cur->child[3]].data);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 8);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[3]].replace, 0);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_RVRI + (u64)TokenType::__end:
					data->instructionValue[cur].data = ((instructionBase[(TokenType)cur->child[0]->type] + data->instructionValue[cur->child[1]].data) << 24) + (data->instructionValue[cur->child[2]].data << 16) + (data->instructionValue[cur->child[3]].data);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 24);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[3]].replace, 0);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_FVRI + (u64)TokenType::__end:
					data->instructionValue[cur].data = ((instructionBase[(TokenType)cur->child[0]->type] + flagBase[(TokenType)cur->child[1]->child[0]->type]) << 24) + (data->instructionValue[cur->child[2]].data << 16) + (data->instructionValue[cur->child[3]].data);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[3]].replace, 0);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_IXP + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + (data->instructionValue[cur->child[1]].data << 8) + (data->instructionValue[cur->child[2]].data);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 8);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 0);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_RXR + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + (data->instructionValue[cur->child[1]].data << 16) + (data->instructionValue[cur->child[2]].data);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 0);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_CTL + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + data->instructionValue[cur->child[1]].data;
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 0);
					goto pushInstruction;
				case (u64)ASTNodeType::instruction_CACHE + (u64)TokenType::__end:
					data->instructionValue[cur].data = (instructionBase[(TokenType)cur->child[0]->type] << 24) + (data->instructionValue[cur->child[1]].data << 16) + (data->instructionValue[cur->child[2]].data << 8) + (data->instructionValue[cur->child[3]].data);
					if ((TokenType)cur->child[0]->type == TokenType::flush)
						data->instructionValue[cur].data |= cacheFlushBit;
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 16);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[2]].replace, 8);

				pushInstruction:
					registerReplace(data, sectionId, data->instructionValue[cur].replace, 4);
					push32(data->sections[sectionId].data, data->instructionValue[cur].data);
					break;

				case (u64)ASTNodeType::reg_ex + (u64)TokenType::__end:
					data->instructionValue[cur].data = regExBase[(TokenType)cur->child[0]->type];
					break;
				case (u64)ASTNodeType::reg + (u64)TokenType::__end:
					data->instructionValue[cur].data = (data->instructionValue[cur->child[0]].data << 3) + data->instructionValue[cur->child[1]].data;
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[0]].replace, 3);
					appendReplace(data->instructionValue[cur].replace, data->instructionValue[cur->child[1]].replace, 0);
					break;
				case (u64)ASTNodeType::regid + (u64)TokenType::__end:
					data->instructionValue[cur].data = regBase[(TokenType)cur->child[0]->type];
					break;
				case (u64)ASTNodeType::reg_index + (u64)TokenType::__end: {
					if (data->expressionValue.find(cur->child[1]) == data->expressionValue.end())
						return (int)EvaluateError::MISSING_EXPRESSION;

					ExprData& expr = data->expressionValue[cur->child[1]];
					if (expr.type == ExprType::CONSTANT)
						data->instructionValue[cur].data = regBase[(TokenType)cur->child[0]->type] + expr.value;
					else {
						ReplaceData rep = {};
						rep.index = data->operationVector.size();
						rep.offset = 0;
						rep.size = 5;
						data->instructionValue[cur].data = regBase[(TokenType)cur->child[0]->type];
						data->instructionValue[cur].replace.push_back(rep);
						data->operationVector.push_back(expr.operations);
					}
					break;
				}
				case (u64)ASTNodeType::regmode + (u64)TokenType::__end:
					data->instructionValue[cur].data = regBase[(TokenType)cur->child[0]->type];
					break;

				case (u64)ASTNodeType::ctlid + (u64)TokenType::__end:
					data->instructionValue[cur].data = ctlBase[(TokenType)cur->child[0]->type];
					break;
				case (u64)ASTNodeType::cacheid + (u64)TokenType::__end:
					data->instructionValue[cur].data = 0;
					for (auto it = cur->child.cbegin(); it != cur->child.cend(); it++)
						data->instructionValue[cur].data |= cacheBase[(TokenType)(*it)->type];
					break;

				case (u64)ASTNodeType::immidate16 + (u64)TokenType::__end: {
					if (data->expressionValue.find(cur->child[0]) == data->expressionValue.end())
						return (int)EvaluateError::MISSING_EXPRESSION;

					ExprData& expr = data->expressionValue[cur->child[0]];
					if (expr.type == ExprType::CONSTANT)
						data->instructionValue[cur].data = expr.value & 0xFFFF;
					else {
						ReplaceData rep = {};
						rep.index = data->operationVector.size();
						rep.offset = 0;
						rep.size = 16;
						data->instructionValue[cur].data = 0;
						data->instructionValue[cur].replace.push_back(rep);
						data->operationVector.push_back(expr.operations);
					}
					break;
				}
				case (u64)ASTNodeType::immidate8 + (u64)TokenType::__end: {
					if (data->expressionValue.find(cur->child[0]) == data->expressionValue.end())
						return (int)EvaluateError::MISSING_EXPRESSION;

					ExprData& expr = data->expressionValue[cur->child[0]];
					if (expr.type == ExprType::CONSTANT)
						data->instructionValue[cur].data = expr.value & 0xFF;
					else {
						ReplaceData rep = {};
						rep.index = data->operationVector.size();
						rep.offset = 0;
						rep.size = 8;
						data->instructionValue[cur].data = 0;
						data->instructionValue[cur].replace.push_back(rep);
						data->operationVector.push_back(expr.operations);
					}
					break;
				}
				}

				return (int)EvaluateError::SUCCESS;
			}

			inline int collapseExpr(EvalData* data) {
				std::unordered_map<std::vector<OperationToken>, size_t, HashOperation, EqualOperation> checked;
				u32 compressIndex = 0;
				for (size_t i = 0; i < data->operationVector.size(); i++) {
					auto& it = data->operationVector[i];
					auto found = checked.find(it);
					if (found == checked.end()) {
						checked.insert({it, i});
						data->compressionIndex.push_back(compressIndex++);
						continue;
					}
					data->compressionIndex.push_back((u32)-1);

					size_t idx = found->second;
					for (auto si = data->sections.begin(); si != data->sections.end(); ++si) {
						for (auto ti = si->operationMap.begin(); ti != si->operationMap.end(); ++ti) {
							if (ti->second.index == i)
								ti->second.index = idx;
						}
					}
				}
				return (int)EvaluateError::SUCCESS;
			}
		}

		Evaluator evaluator;

		inline void setTree(AST* tree) {
			evaluator.setTree(tree);
		}

		// on failure errorTok is the first source token of the failing node (line = -1 if none)
		inline int evaluate(lexer::Token& errorTok, functions::EvalData& data) {
			int code = evaluator.evaluate(errorTok, functions::calculateSection, &data);
			if (code) return code;
			code = evaluator.evaluate(errorTok, functions::calculateAddr, &data);
			if (code) return code;
			code = evaluator.evaluate(errorTok, functions::evaluateCode, &data);
			if (code) return code;
			code = functions::collapseExpr(&data);
			if (code) {
				errorTok = {};
				errorTok.line = -1;
				return code;
			}

			return (int)functions::EvaluateError::SUCCESS;
		}
	}

	namespace format {
		using u32 = unsigned __int32;
		using u8 = unsigned __int8;

		using EvalData = evaluator::functions::EvalData;
		using Format = format_data::Format;
		using Section = format_data::Section;
		using Operation = format_data::Operation;
		using Mapper = format_data::Mapper;
		using Identifier = format_data::Identifier;

		Format format;

		inline bool formatData(const EvalData& data) {
			format.clear();

			// section names are file-local like labels (so the linker only merges same-named
			// sections within one file), unless declared with `global section`
			auto sectionName = [&data](const std::string& name) {
				if (data.globalSectionIdentifier.find(name) != data.globalSectionIdentifier.end())
					return name;
				return evaluator::functions::mangleLabel(data.fileName, name);
			};
			format.endian = bigEndian ? format_data::Endian::BIG : format_data::Endian::LITTLE;

			std::unordered_map<std::string, u32> usedIdentifierIndex;

			for (size_t i = 0; i < data.operationVector.size(); i++) {
				if (data.compressionIndex[i] == (u32)-1)
					continue;

				std::vector<Operation> copy;
				for (auto si = data.operationVector[i].cbegin(); si != data.operationVector[i].cend(); ++si) {
					Operation nOper = {};
					nOper.type = (u32)si->type;

					if (si->type == evaluator::functions::OperationType::IDENTIFIER) {
						// global (defined here) and extern (defined elsewhere) names are linked by their
						// plain name; everything else is file-local and gets the file prefix
						std::string mangled = si->text;
						if (data.globalIdentifier.find(si->text) == data.globalIdentifier.end() && data.externIdentifier.find(si->text) == data.externIdentifier.end())
							mangled = evaluator::functions::mangleLabel(data.fileName, si->text);
						auto find = usedIdentifierIndex.find(mangled);
						if (find == usedIdentifierIndex.end()) {
							u32 idx = (u32)format.usedIdentifier.size();
							usedIdentifierIndex[mangled] = idx;
							format.usedIdentifier.push_back(mangled);
							nOper.value = idx;
						}
						else
							nOper.value = find->second;
					}
					else
						nOper.value = si->value;

					copy.push_back(nOper);
				}

				Operation eox = {};
				eox.type = (u32)evaluator::functions::OperationType::EOX;
				eox.value = 0;
				copy.push_back(eox);

				format.expressions.push_back(copy);
			}

			for (auto it = data.idenifierValue.cbegin(); it != data.idenifierValue.cend(); ++it) {
				Identifier nId = {};
				nId.name = it->first;
				if (data.globalIdentifier.find(it->first) == data.globalIdentifier.end())
					nId.name = evaluator::functions::mangleLabel(data.fileName, it->first);
				if (it->second.coeff.empty()) {
					nId.section = "";
					nId.sectionIndex = (u32)-1;
				}
				else {
					size_t section_id = it->second.coeff.cbegin()->first;
					nId.section = sectionName(data.sections[section_id].name);
					nId.sectionIndex = (u32)section_id;
				}
				nId.value = (u32)it->second.value;
				format.definedIdentifier.push_back(nId);
			}

			u32 base = 0;
			for (auto it = data.sections.cbegin(); it != data.sections.cend(); ++it) {
				base = (u32)format.binary.size();

				for (auto si = it->operationMap.cbegin(); si != it->operationMap.cend(); ++si) {
					Mapper nMap = {};
					nMap.sectionName = sectionName(it->name);
					nMap.byteIndex = base + si->first;
					nMap.offset = si->second.offset;
					nMap.size = si->second.size;
					// index is into the raw operationVector; format.expressions skips duplicates,
					// so translate to the position in that compacted list
					nMap.exprIndex = data.compressionIndex[si->second.index];

					format.mapper.push_back(nMap);
				}

				Section nSec = {};
				nSec.name = sectionName(it->name);
				nSec.type = (u32)it->type;
				nSec.start = base;
				nSec.end = base + (u32)it->data.size();
				// label offsets are counted in sectionByte, which also covers .bss reservations
				// that put no bytes in data
				nSec.size = data.sectionByte[(size_t)(it - data.sections.cbegin())];
				format.sections.push_back(nSec);
				format.binary.insert(format.binary.end(), it->data.begin(), it->data.end());
			}

			return true;
		}

		inline void getRaw(std::vector<u8>& out) {
			format.getRaw(out);
		}
	}

	const unsigned long long version = 415;

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
			using GraphID = lexer_generator::graph::ID;

			u64 tableSize = *it; it++;
			u64 endDataSize = *it; it++;

			GraphID** table = new GraphID * [tableSize];
			for (size_t i = 0; i < tableSize; i++) {
				table[i] = new GraphID[128];
				for (size_t j = 0; j < 128; j++) {
					table[i][j] = (GraphID)*it;
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
				out.endData.push_back({ (GraphID)endData[i][0], { (lexer::TokenType)endData[i][1], endData[i][2] } });
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

			using Element = parser_generator::convert::Element;

			for (size_t i = 0; i < vectorSize; i++) {
				Element head = (Element)*it; it++;
				std::vector<Element> tmp;
				for (size_t j = 0; j < vectorTailSize[i]; j++) {
					tmp.push_back((Element)*it); it++;
				}
				out.grammerVec.push_back({ head, tmp });
			}

			for (size_t i = 0; i < vectorSize; i++) {
				Element head1 = (Element)*it; it++;
				int head2 = (int)*it; it++;
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
		// each flag must be a distinct bit -- chaining "X << 1" starting from a base of 0
		// (as this used to do) leaves every subsequent value at 0 too (0 << 1 is still 0),
		// so every flag ends up indistinguishable from "none"/"SUCCESS" and all the
		// `dump.flags & getX` / `dump.errorCode & FAIL_X` checks silently never match.
		enum AssemblerError {
			SUCCESS = 0,
			FAIL_LEX = 1,
			FAIL_PREPROCESS = FAIL_LEX << 1,
			FAIL_PARSE = FAIL_PREPROCESS << 1,
			FAIL_EVALUATE = FAIL_PARSE << 1,
			FAIL_FORMAT = FAIL_EVALUATE << 1
		};

		enum flag {
			none = 0,
			getToken = 1,
			getPreproc = getToken << 1,
			getParse = getPreproc << 1,
			getAST = getParse << 1,
			getEval = getAST << 1,
			getBin = getEval << 1
		};

		u32 flags = 0;

		int errorCode = 0;
		std::vector<lexer::Token> tokens;
		std::vector<preprocesser::Token> preprocTokens;
		parser::Parser::ASTNode* pAST;
		std::vector<evaluator::functions::Section> sectionDump;
		std::string errorMessage;
	} AssemblerDump;

	typedef int(*getFileIO)(std::string&, std::string);

	getFileIO fileIoFunc;

	int assemble(const std::string& text, std::vector<u8>& binary, AssemblerDump& dump, std::string fileName, std::string outputMangle) {
		using TokenType = lexer::TokenType;
		using Token = lexer::Token;
		using Parser = parser::Parser;

		
		std::vector<Token> tokens;
		// ordered multimap: equal keys keep insertion order, so a line's tokens read back in source order
		std::multimap<int, Token> tokenBackup;
		std::vector<std::string> includedFiles = { preprocesser::normalizePath(fileName) };
		int line = 0;
		int fileId = 0;

		preprocesser::functions::PreprocData preprocData;
		preprocData.state = preprocesser::functions::State::normal;
		// needed so includeCallback can resolve #include<...> paths relative to this file's own
		// directory, and so a file #include-ing itself (directly or via a cycle) is caught.
		preprocData.fileName = fileName;
		preprocData.includedFiles.insert(preprocesser::normalizePath(fileName));
		int preprocessError = 0;
		
		// initial lex/preprocess
		// tag BEFORE backing up -- tokenBackup takes independent copies, so tagging tokens
		// afterward would never reach them, leaving .line/.fileLine/.fileId uninitialized there.
		lexer::lexer.lex(tokens, text);
		line = preprocesser::tagLine(tokens);
		for (auto it = tokens.begin(); it != tokens.end(); ++it) 
			tokenBackup.insert({it->line, *it});

		preprocesser::functions::includeCallback = [&](std::vector<Token>& tokOut, std::string path, preprocesser::functions::PreprocData* preprocData) -> int {
			//get file -- resolve the #include<path> relative to fileName's own directory
			// normalize before comparing/storing so "A/B/../C" and "A/C" (or "A/./C") are
			// recognized as the same file for duplicate/circular #include detection.

			std::string baseDir = preprocData->fileName;
			size_t baseSlash = baseDir.find_last_of("/\\");
			baseDir = (baseSlash == std::string::npos) ? "" : baseDir.substr(0, baseSlash + 1);
			// reassign the parameter directly rather than redeclaring -- a redeclared local of the
			// same name would shadow the parameter and read itself (uninitialized) on the right-hand
			// side instead of the actual #include<...> path.
			path = preprocesser::normalizePath(baseDir + path);

			if (preprocData->includedFiles.find(path) != preprocData->includedFiles.end())
				return (int)preprocesser::functions::PreprocessError::DUPLICATE_INCLUDE;

			std::string curText;
			std::vector<Token> curTokens;

			// getText
			if(fileIoFunc(curText, path) != 0)
				return (int)preprocesser::functions::PreprocessError::INVALID_INCLUDE_PATH;
			if (curText.empty() || curText.back() != '\n')
				curText.push_back('\n');
			preprocData->includedFiles.insert(path);
			includedFiles.push_back(path);
			fileId++;

			// lex
			lexer::lexer.lex(curTokens, curText);

			// preprocess -- tag before backing up, same reasoning as the initial lex/preprocess above
			line = preprocesser::tagLine(curTokens, line, fileId);
			for (auto it = curTokens.begin(); it != curTokens.end(); ++it) 
				tokenBackup.insert({it->line, *it});

			preprocesser::setTokensRem(curTokens);
			lexer::Token remErrorTok = {};
			int code = preprocesser::preprocessRem(remErrorTok, *preprocData);
			if (code) return code;

			tokOut = curTokens;
			return (int)preprocesser::functions::PreprocessError::SUCCESS;
		};

		// "<file>:<line>: error: <detail>" + the source line, for a token tagged by tagLine()
		// (a token without a line -- e.g. end of input -- reports just the main file)
		auto describeError = [&](const Token& tok, const std::string& detail) {
			if (tok.line < 0)
				return iotools::Terminal::codeError(includedFiles[0], detail);
			std::string file = (tok.fileId >= 0 && tok.fileId < (int)includedFiles.size()) ? includedFiles[tok.fileId] : std::to_string(tok.fileId);
			std::string msg = iotools::Terminal::codeError(iotools::errPosGenerator(file, tok.fileLine), detail);
			std::string source;
			auto range = tokenBackup.equal_range(tok.line);
			for (auto it = range.first; it != range.second; ++it)
				if (it->second.type != TokenType::newline)
					source += it->second.text;
			size_t first = source.find_first_not_of(" \t");
			if (first != std::string::npos)
				msg += "\n    " + source.substr(first);
			return msg;
		};

		Token errorTok = {};
		preprocesser::setTokensRem(tokens);
		preprocessError = preprocesser::preprocessRem(errorTok, preprocData);
		if (!preprocessError) {
			preprocesser::setTokensRep(tokens);
			preprocessError = preprocesser::preprocessRep(errorTok, preprocData);
		}

		if (dump.flags & AssemblerDump::getToken) {
			dump.tokens.reserve(tokenBackup.size());
			for (auto it = tokenBackup.cbegin(); it != tokenBackup.cend(); ++it)
				dump.tokens.push_back(it->second);
		}

		bool validPreprocess = preprocessError == 0;
		dump.errorCode |= (validPreprocess) ? 0 : (int)AssemblerDump::FAIL_PREPROCESS;

		if (dump.flags & AssemblerDump::getPreproc || !validPreprocess) {
			dump.preprocTokens = tokens;
		}

		if (!validPreprocess) {
			std::string detail = preprocesser::functions::preprocessErrorName(preprocessError);
			if (errorTok.line >= 0)
				detail += ": " + errorTok.text;
			dump.errorMessage = describeError(errorTok, detail);
			return -1;
		}

		Parser::ASTNode* pAST;
		errorTok = {};
		bool validParse = parser::parser.parse(errorTok, pAST, tokens);
		dump.errorCode |= (validParse) ? 0 : (int)AssemblerDump::FAIL_PARSE;

		if (dump.flags & AssemblerDump::getAST || !validParse) {
			dump.pAST = pAST;
		}

		if (dump.flags & AssemblerDump::getEval || !validParse) {
			if (!validParse) {
				dump.errorMessage = describeError(errorTok, "unexpected token: " + errorTok.text);
			}
		}

		if (!validParse) {
			parser::parser.delAST(pAST);
			return -1;
		}

		evaluator::functions::EvalData data;
		data.fileName = outputMangle;
		evaluator::setTree(pAST);
		int evaluateError = evaluator::evaluate(errorTok, data);
		bool validEvaluate = evaluateError == 0;
		dump.errorCode |= (validEvaluate) ? 0 : (int)AssemblerDump::FAIL_EVALUATE;

		if (!validEvaluate) {
			dump.errorMessage = describeError(errorTok, std::string(evaluator::functions::evaluateErrorName(evaluateError)) + ": " + errorTok.text);
			parser::parser.delAST(pAST);
			return -1;
		}

		std::vector<u8> raw8;
		format::formatData(data);
		format::getRaw(raw8);

		if (dump.flags & AssemblerDump::getBin) {
			dump.sectionDump = data.sections;
		}

		parser::parser.delAST(pAST);
		binary = raw8;

		return 0;
	}
}

#endif