#ifndef DPICP_STACKMACHINE
#define DPICP_STACKMACHINE

#include <vector>

namespace stack_machine_generator {

	template<class Operation>
	class StackMachine {
	public:
		using u32 = unsigned __int32;

		typedef struct _Element {
			Operation type;
			u32 value;
		} Element;

		// function(ret, opType, args): computes ret from args.size() operands, returns 0 on success;
		// any other value is an error code that run() propagates unchanged.
		typedef int(*StackMachineFunction)(u32&, Operation, const std::vector<u32>&);

		// run()'s own error codes (as opposed to ones returned by `function`, which pass through
		// unchanged): 0 = success.
		enum class RunError {
			SUCCESS = 0,
			STACK_UNDERFLOW, // an operator needed more operands than were on the run stack
			INVALID_RESULT   // after processing every element, the stack didn't end with exactly one value
		};

		// src: postfix (RPN) element list. Operation::CONSTANT elements are leaves pushed as-is;
		// any other element pops `value` operands off the run stack, calls `function`, and pushes the result as CONSTANT.
		int run(const std::vector<Element>& src, StackMachineFunction function, u32& result) const {
			std::vector<Element> runstack;

			for (auto it = src.cbegin(); it != src.cend(); ++it) {
				if (it->type == Operation::CONSTANT) {
					runstack.push_back(*it);
					continue;
				}

				if (runstack.size() < it->value)
					return (int)RunError::STACK_UNDERFLOW;

				std::vector<u32> args;
				for (size_t i = runstack.size() - it->value; i < runstack.size(); i++)
					args.push_back(runstack[i].value);
				runstack.resize(runstack.size() - it->value);

				Element ret = {};
				ret.type = Operation::CONSTANT;
				int code = function(ret.value, it->type, args);
				if (code) return code;

				runstack.push_back(ret);
			}

			if (runstack.size() != 1)
				return (int)RunError::INVALID_RESULT;

			result = runstack.back().value;
			return (int)RunError::SUCCESS;
		}
	};
}

#endif
