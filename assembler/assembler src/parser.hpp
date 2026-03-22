#ifndef DPICP_PARSER
#define DPICP_PARSER

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

#include "lexer.hpp"

#define NTSTART(name) enum class name { __accept, 
#define NTEND() __end }

namespace parser_generator {
	namespace convert {
		using u64 = unsigned __int64;
		using ID = u64;
		using Element = u64;

		typedef struct _Item {
			u64 prodId;
			u64 dotPos;
			u64 lookaheadId;
		} Item;

		inline void getClosure(
			std::unordered_set<ID>& reduceItems,
			std::vector<Item>& closure,
			std::vector<std::pair<Element, std::vector<Element>>>& grammerVec,
			std::unordered_multimap<Element, std::pair<std::vector<Element>, u64>>& grammerMap,
			std::vector<Item> items) {

			typedef struct _QData {
				Item item;
				Item prev;
			} QData;

			std::unordered_set<Element> visited;

			std::queue<QData> itemQ;
			for (auto it = items.begin(); it != items.end(); ++it) {
				if (grammerVec[it->prodId].second.size() == it->dotPos) {
					it->lookaheadId = 0;
					continue;
				}

				it->lookaheadId = (u64)-1;
				itemQ.push({ *it, *it });
			}

			while (!itemQ.empty()) {
				QData cur = itemQ.front(); itemQ.pop();
				
				if (grammerVec[cur.item.prodId].second.size() == cur.item.dotPos)
					continue;
				Element mark = grammerVec[cur.item.prodId].second[cur.item.dotPos];
				Element lhs = grammerVec[cur.item.prodId].first;
				if (visited.find(mark) != visited.end())
					continue;
				visited.insert(mark);

				auto range = grammerMap.equal_range(mark);
				for (auto it = range.first; it != range.second; ++it) {
					Item tmp;
					tmp.prodId = it->second.second;
					tmp.dotPos = 0;
					tmp.lookaheadId = (it->second.first.size()) ? (u64)-1 : 0;

					itemQ.push({ tmp, cur.item });
					closure.push_back(tmp);
				}
			}

			for (auto it = items.begin(); it != items.end(); ++it) {
				if (visited.find(grammerVec[it->prodId].first) == visited.end() || it->dotPos)
					closure.push_back(*it);
			}

			size_t counter = closure.size();
			for (auto it = closure.rbegin(); it != closure.rend(); ++it) {
				counter--;
				if (!it->lookaheadId) {
					reduceItems.insert(counter);
				}
			}
		}

		inline void getGotoItem(
			std::unordered_map<Element, std::vector<Item>>& gotoMap,
			std::vector<std::pair<Element, std::vector<Element>>>& grammerVec,
			std::vector<Item>& closure) {
			
			for (auto it = closure.begin(); it != closure.end(); ++it) {
				if (it->dotPos + 1 > grammerVec[it->prodId].second.size())
					continue;
				gotoMap[grammerVec[it->prodId].second[it->dotPos]].push_back({ it->prodId, it->dotPos + 1 });
			}
		}

		typedef struct _SetLessRule {
			bool operator() (const std::vector<Item>& left, const std::vector<Item>& right) const {
				if (left.size() < right.size())
					return true;
				if (left.size() > right.size())
					return false;

				auto li = left.cbegin(), ri = right.cbegin();
				while (li != left.cend() && ri != right.cend()) {
					if (li->prodId != ri->prodId) {
						if (li->prodId < ri->prodId)
							return true;
						return false;
					}
					if (li->dotPos < ri->dotPos)
						return true;

					++li; ++ri;
				}
				return false;
			}
		} SetLessRule;

		inline void createC0(
			std::unordered_map<ID, std::unordered_set<ID>>& reduceItem,
			std::vector<std::pair<ID, ID>>& startItemIds,
			std::vector<std::vector<Item>>& closureVec,
			std::unordered_multimap<ID, std::pair<ID, Element>>& transitionMap,
			std::vector<std::pair<Element, std::vector<Element>>>& grammerVec,
			std::unordered_multimap<Element, std::pair<std::vector<Element>, u64>>& grammerMap,
			Item startItem) {

			typedef struct _QData {
				std::vector<Item> closure;
				ID id;
			} QData;

			ID closureId = 0;
			std::map<std::vector<Item>, ID, SetLessRule> visited;

			std::queue<QData> setQ;
			std::vector<Item> startClosure;
			getClosure(reduceItem[closureId], startClosure, grammerVec, grammerMap, { startItem });
			setQ.push({ startClosure, closureId });
			closureVec.push_back(startClosure);
			visited[startClosure] = closureId++;

			

			while (!setQ.empty()) {
				QData cur = setQ.front(); setQ.pop();

				std::unordered_map<Element, std::vector<Item>> gotoMap;
				getGotoItem(gotoMap, grammerVec, cur.closure);
				for (auto it = gotoMap.begin(); it != gotoMap.end(); ++it) {
					std::vector<Item> closure;
					getClosure(reduceItem[closureId], closure, grammerVec, grammerMap, it->second);

					if (visited.find(closure) != visited.end()) {
						reduceItem[closureId].clear();

						ID id = visited[closure];
						transitionMap.insert({ cur.id, { id, it->first} });
						continue;
					}
					ID id = closureId++;
					visited[closure] = id;
					closureVec.push_back(closure);
					transitionMap.insert({ cur.id, { id, it->first} });

					setQ.push({ closure, id });
				}
			}

			size_t closureIndex = 0;
			for (auto it = closureVec.begin(); it != closureVec.end(); ++it) {
				size_t itemIndex = 0;
				for (auto si = it->begin(); si != it->end(); ++si) {
					if (si->prodId == startItem.prodId)
						startItemIds.push_back({ closureIndex, itemIndex });
					itemIndex++;
				}
				closureIndex++;
			}
		}

		inline std::vector<u64>& getFirst(
			Element element,
			std::unordered_multimap<Element, std::pair<std::vector<Element>, u64>>& grammerMap,
			std::unordered_map<Element, std::vector<u64>>& firstCache,
			Element terminalEnd) {

			if (element < terminalEnd) {
				if (firstCache.find(element) == firstCache.end()) {
					firstCache[element].resize((element >> 6) + 1, 0);
					firstCache[element][element >> 6] |= (u64)1 << (element & 0b111111);
				}
				return firstCache[element];
			}

			typedef struct _SData {
				Element cur;
				Element prev;
				size_t index;
			} SData;

			std::unordered_set<Element> calculating;

			std::stack<SData> elementStack;
			elementStack.push({ element, 0 });

			while (!elementStack.empty()) {
				SData cur = elementStack.top();

				if (cur.cur < terminalEnd && firstCache.find(cur.cur) == firstCache.end()) {
					firstCache[cur.cur].resize(std::max(firstCache[cur.cur].size(), (size_t)(cur.cur >> 6) + 1), 0);
					firstCache[cur.cur][cur.cur >> 6] |= (u64)1 << (cur.cur & 0b111111);
				}

				if (firstCache.find(cur.cur) != firstCache.end()) {
					calculating.erase(cur.cur);

					firstCache[cur.prev].resize(std::max(firstCache[cur.prev].size(), firstCache[cur.cur].size()), 0);
					firstCache[cur.prev][0] &= ~((u64)0b1);
					for (size_t i = 0; i < firstCache[cur.cur].size(); i++)
						firstCache[cur.prev][i] |= firstCache[cur.cur][i];

					elementStack.pop();
					if (!(firstCache[cur.cur].size() && (firstCache[cur.cur][0] & 0b1))) {
						while (!elementStack.empty() && elementStack.top().index == cur.index)
							elementStack.pop();
					}

					continue;
				}

				if (calculating.find(cur.cur) != calculating.end()) {
					firstCache[cur.prev].resize(std::max(firstCache[cur.prev].size(), (size_t)1), 0);
					if (!(firstCache[cur.cur][0] & 0b1)) {
						while (!elementStack.empty() && elementStack.top().index == cur.index)
							elementStack.pop();
					}
					continue;
				}

				calculating.insert(cur.cur);

				size_t nextIndex = cur.index + 1;
				auto range = grammerMap.equal_range(cur.cur);
				for (auto it = range.first; it != range.second; ++it) {
					for (auto si = it->second.first.rbegin(); si != it->second.first.rend(); ++si)
						elementStack.push({ *si, cur.cur, nextIndex });
					nextIndex++;
				}

				if (range.first == range.second) {
					elementStack.push({ 0, cur.cur, nextIndex });
				}
			}

			return firstCache[element];
		}

		inline void lookaheadHelper(
			std::vector<u64>& lookahead,
			std::vector<std::pair<u64, u64>>& ref,
			std::vector<std::vector<Item>>& closureVec,
			std::unordered_map<Element, std::unordered_multimap<ID, ID>>& revTransitionMap,
			std::vector<std::pair<Element, std::vector<Element>>>& grammerVec,
			std::unordered_multimap<Element, std::pair<std::vector<Element>, u64>>& grammerMap,
			std::unordered_map<Element, std::vector<u64>>& firstCache,
			Element terminalEnd, ID closureID, Item item) {

			std::pair<Element, std::vector<Element>>& core = grammerVec[item.prodId];
			std::vector<Element> prev;
			for (size_t i = 0; i < item.dotPos; i++)
				prev.push_back(core.second[i]);

			typedef struct _QData {
				ID closureId;
				u64 index;
			} QData;

			std::unordered_set<ID> predSet;
			std::queue<QData> closureQ;

			closureQ.push({ closureID, (u64)(item.dotPos - 1)});
			while (!closureQ.empty()) {
				QData cur = closureQ.front(); closureQ.pop();

				if (cur.index == (u64)-1) {
					predSet.insert(cur.closureId);
					continue;
				}

				auto range = revTransitionMap[prev[cur.index]].equal_range(cur.closureId);
				for (auto it = range.first; it != range.second; ++it)
					closureQ.push({ it->second, cur.index - 1 });
			}

			for (auto it = predSet.begin(); it != predSet.end(); ++it) {
				std::vector<Item>& curClosure = closureVec[*it];
				u64 counter = 0;
				for (auto si = curClosure.begin(); si != curClosure.end(); ++si) {
					u64 itemId = counter++;
					if (grammerVec[si->prodId].second.size() <= si->dotPos || grammerVec[si->prodId].second[si->dotPos] != core.first)
						continue;

					std::vector<Element>& rhs = grammerVec[si->prodId].second;

					std::vector<Element> follow;
					for (size_t i = si->dotPos + 1; i < rhs.size(); i++)
						follow.push_back(rhs[i]);

					bool continueFlag = true;
					size_t index = 0;
					do {
						if (index >= follow.size())
							break;

						std::vector<u64>& first = getFirst(follow[index], grammerMap, firstCache, terminalEnd);
						lookahead.resize(std::max(lookahead.size(), first.size()), 0);
						for (size_t i = 0; i < first.size(); i++)
							lookahead[i] |= first[i];
						lookahead[0] &= ~((u64)0b1);

						continueFlag = first[0] & 0b1; index++;
					} while (continueFlag);

					if (continueFlag)
						ref.push_back({ *it, itemId });
				}
			}
		}

		typedef struct _DoubleID {
			ID closureId;
			ID itemId;

			operator std::pair<ID, ID>() const {
				return { closureId, itemId };
			}

			bool operator == (const _DoubleID& other) const {
				return (this->closureId == other.closureId && this->itemId == other.itemId);
			}
		} DoubleID;

		typedef struct _HashDoubleID {
			size_t operator()(const DoubleID& k) const {
				size_t h1 = std::hash<ID>()(k.closureId);
				size_t h2 = std::hash<ID>()(k.itemId);
				return h1 ^ (h2 << 32) ^ (h2 >> 32);
			}
		} HashDoubleID;

		typedef struct _TarjanDFSdata {
			std::unordered_map<DoubleID, u64, HashDoubleID>& index;
			std::unordered_map<DoubleID, u64, HashDoubleID>& low;
			std::unordered_map<DoubleID, bool, HashDoubleID>& onStack;
			std::unordered_multimap<DoubleID, DoubleID, HashDoubleID>& transitionMap;
			u64 indexCt;
		} TarjanDFSdata;

		inline void tarjanDFS(TarjanDFSdata& data, DoubleID id) {
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
			std::unordered_map<DoubleID, DoubleID, HashDoubleID>& sccMap,
			std::unordered_multimap<DoubleID, DoubleID, HashDoubleID>& transitionMap) {

			std::unordered_map<DoubleID, u64, HashDoubleID> index, low;
			std::unordered_map<DoubleID, bool, HashDoubleID> onStack;

			TarjanDFSdata data = { index, low, onStack, transitionMap, 1 };

			std::unordered_set<DoubleID, HashDoubleID> idSet;
			for (auto it = transitionMap.begin(); it != transitionMap.end(); ++it) {
				idSet.insert(it->first);
				idSet.insert(it->second);
			}

			for (auto it = idSet.begin(); it != idSet.end(); ++it) {
				if (index[*it])
					continue;

				tarjanDFS(data, *it);
			}

			std::unordered_map<ID, DoubleID> rIndex;
			for (auto it = index.begin(); it != index.end(); ++it)
				rIndex[it->second] = it->first;

			for (auto it = idSet.begin(); it != idSet.end(); ++it)
				sccMap[*it] = rIndex[low[*it]];
		}

		inline DoubleID replaceScc(DoubleID id, std::unordered_map<DoubleID, DoubleID, HashDoubleID>& sccMap) {
			auto it = sccMap.find(id);
			if (it == sccMap.end())
				return id;
			return it->second;
		}

		inline void getLookahead(
			std::vector<std::vector<u64>>& lookaheadSets,
			std::vector<std::vector<Item>>& closureVec,
			std::unordered_map<ID, std::unordered_set<ID>>& reduceItem,
			std::vector<std::pair<ID, ID>>& startItems,
			std::unordered_map<Element, std::unordered_multimap<ID, ID>>& revTransitionMap,
			std::vector<std::pair<Element, std::vector<Element>>>& grammerVec,
			std::unordered_multimap<Element, std::pair<std::vector<Element>, u64>>& grammerMap,
			Element terminalEnd, Element eot) {

			typedef struct _TableData {
				std::vector<std::pair<ID, ID>> ref;
				std::vector<u64> lookaheadSet;
			} TableData;

			std::unordered_map<Element, std::vector<u64>> firstCache;
			std::unordered_map<ID, std::unordered_map<ID, TableData>> lookaheadTable;
			std::unordered_map<ID, std::unordered_set<ID>> visited;
			std::unordered_multimap<DoubleID, DoubleID, HashDoubleID> rRefMap;

			typedef struct _QData {
				ID closureId, itemId;
			} QData;

			std::queue<QData> itemQ;

			for (auto it = reduceItem.begin(); it != reduceItem.end(); ++it) {
				for (auto si = it->second.begin(); si != it->second.end(); ++si)
					itemQ.push({ it->first, *si });
			}

			u64 count = 0;
			while (!itemQ.empty()) {
				QData cur = itemQ.front(); itemQ.pop();
				if (!visited[cur.closureId].insert(cur.itemId).second)
					continue;
				
				std::vector<Item>& closure = closureVec[cur.closureId];
				lookaheadHelper(lookaheadTable[cur.closureId][cur.itemId].lookaheadSet, lookaheadTable[cur.closureId][cur.itemId].ref, closureVec, revTransitionMap, grammerVec, grammerMap, firstCache, terminalEnd, cur.closureId, closure[cur.itemId]);

				std::vector<std::pair<ID, ID>>& ref = lookaheadTable[cur.closureId][cur.itemId].ref;
				for (auto it = ref.begin(); it != ref.end(); ++it) {
					itemQ.push({ it->first, it->second });
					rRefMap.insert({ { it->first, it->second }, { cur.closureId, cur.itemId } });
				}
			}

			for (auto it = startItems.begin(); it != startItems.end(); ++it) {
				lookaheadTable[it->first][it->second].ref.clear();
				std::vector<u64>& startLook = lookaheadTable[it->first][it->second].lookaheadSet;
				startLook.resize((eot >> 6) + 1, 0);
				startLook[eot >> 6] |= (u64)1 << (eot & 0b111111);
			}

			std::unordered_map<DoubleID, DoubleID, HashDoubleID> sccMap;
			std::unordered_multimap<DoubleID, DoubleID, HashDoubleID> sccRRefMap;
			std::set<std::pair<std::pair<ID, ID>, std::pair<ID, ID>>> sccRRefSet;
			tarjan(sccMap, rRefMap);

			typedef struct _DIDC {
				bool operator() (const DoubleID& left, const DoubleID& right) const {
					return (left.closureId < right.closureId || (left.closureId == right.closureId && left.itemId < right.itemId));
				}
			} DIDC;

			for (auto it = rRefMap.begin(); it != rRefMap.end(); ++it) {
				DoubleID first = replaceScc(it->first, sccMap), second = replaceScc(it->second, sccMap);
				if (sccRRefSet.insert({ first, second }).second && !(first == second))
					sccRRefMap.insert({ first, second });
			}

			std::unordered_map<DoubleID, std::unordered_set<DoubleID, HashDoubleID>, HashDoubleID> sccRefTable;

			for (auto it = sccMap.begin(); it != sccMap.end(); ++it) {
				TableData& cur = lookaheadTable[it->first.closureId][it->first.itemId];
				TableData& scc = lookaheadTable[it->second.closureId][it->second.itemId];
				scc.lookaheadSet.resize(std::max(scc.lookaheadSet.size(), cur.lookaheadSet.size()), 0);
				for (size_t i = 0; i < cur.lookaheadSet.size(); i++)
					scc.lookaheadSet[i] |= cur.lookaheadSet[i];

				for (auto si = cur.ref.begin(); si != cur.ref.end(); si++)
					sccRefTable[it->second].insert({ si->first, si->second });
			}

			std::vector<DoubleID> orderedSet;
			std::unordered_map<DoubleID, u64, HashDoubleID> inDegree;
			std::queue<DoubleID> sortQ;

			for (auto it = sccRRefMap.begin(); it != sccRRefMap.end(); ++it) {
				inDegree[it->first];
				inDegree[it->second]++;
			}

			for (auto it = inDegree.begin(); it != inDegree.end(); ++it) {
				if (!it->second)
					sortQ.push(it->first);
			}

			while (!sortQ.empty()) {
				DoubleID cur = sortQ.front(); sortQ.pop();
				orderedSet.push_back(cur);

				auto range = sccRRefMap.equal_range(cur);
				for (auto it = range.first; it != range.second; ++it) {
					if (!--inDegree[it->second])
						sortQ.push(it->second);
				}
			}

			for (auto it = orderedSet.begin(); it != orderedSet.end(); ++it) {
				std::vector<u64>& curSet = lookaheadTable[it->closureId][it->itemId].lookaheadSet;

				auto& ref = sccRefTable[*it];
				for (auto si = ref.begin(); si != ref.end(); ++si) {
					std::vector<u64>& lookaheadSet = lookaheadTable[si->closureId][si->itemId].lookaheadSet;
					curSet.resize(std::max(curSet.size(), lookaheadSet.size()), 0);
					for (u64 i = 0; i < lookaheadSet.size(); i++)
						curSet[i] |= lookaheadSet[i];
				}
			}
			
			for (auto it = sccMap.begin(); it != sccMap.end(); ++it) {
				TableData& cur = lookaheadTable[it->first.closureId][it->first.itemId];
				TableData& scc = lookaheadTable[it->second.closureId][it->second.itemId];

				cur.lookaheadSet = scc.lookaheadSet;
			}

			count = 0;
			for (auto it = lookaheadTable.begin(); it != lookaheadTable.end(); ++it) {
				for (auto si = it->second.begin(); si != it->second.end(); ++si) {
					lookaheadSets.push_back(si->second.lookaheadSet);
					closureVec[it->first][si->first].lookaheadId = count++;
				}
			}
		}

		enum class ActionType {
			Shift,
			Reduce,
			Accept,
			Error
		};

		typedef struct _Action {
			ID arg;
			ActionType type;
		} Action;

		typedef struct _Goto {
			ID state;
			bool error;
		} Goto;

		typedef struct _ParserTable {
			Action** actionTable;
			Goto** gotoTable;
			size_t size;
			size_t actionSize;
			size_t gotoSize;
		} ParserTable;

		void printTable(ParserTable&, int);

		void createTable(ParserTable& table, std::vector<std::pair<Element, std::vector<Element>>>& grammerVec, Element terminalEnd, Element nonterminalEnd, Element acceptNonterminal) {
			acceptNonterminal += terminalEnd;

			Item startItem = {};
			std::unordered_multimap<Element, std::pair<std::vector<Element>, u64>> grammerMap;
			u64 count = 0;
			for (auto it = grammerVec.begin(); it != grammerVec.end(); ++it) {
				grammerMap.insert({ it->first, { it->second, count } });
				if (it->first == acceptNonterminal)
					startItem = { count, 0, (u64)-1 };
				count++;
			}

			std::unordered_map<ID, std::unordered_set<ID>> reduceItem;
			std::vector<std::pair<ID, ID>> startItemIds;
			std::vector<std::vector<Item>> closureVec;
			std::unordered_multimap<ID, std::pair<ID, Element>> transitionMap;
			createC0(reduceItem, startItemIds, closureVec, transitionMap, grammerVec, grammerMap, startItem);

			std::unordered_map<Element, std::unordered_multimap<ID, ID>> revTransitionMap;
			for (auto it = transitionMap.begin(); it != transitionMap.end(); ++it)
				revTransitionMap[it->second.second].insert({ it->second.first, it->first });

			std::vector<std::vector<u64>> lookaheadSets;
			getLookahead(lookaheadSets, closureVec, reduceItem, startItemIds, revTransitionMap, grammerVec, grammerMap, terminalEnd, terminalEnd - 1);

			size_t stateSize = closureVec.size(), actionSize = terminalEnd, gotoSize = nonterminalEnd;
			table.actionTable = new Action*[stateSize];
			table.gotoTable = new Goto*[stateSize];
			table.size = stateSize;
			table.actionSize = actionSize;
			table.gotoSize = gotoSize;

			for (size_t i = 0; i < stateSize; i++) {
				table.actionTable[i] = new Action[actionSize];
				for (size_t j = 0; j < actionSize; j++)
					table.actionTable[i][j] = { 0, ActionType::Error };

				table.gotoTable[i] = new Goto[gotoSize];
				for (size_t j = 0; j < gotoSize; j++)
					table.gotoTable[i][j] = { 0, true };
			}

			std::map<std::pair<ID, ID>, int> handledReduce;

			for (auto it = transitionMap.begin(); it != transitionMap.end(); ++it) {
				if (it->second.second < terminalEnd) {
					table.actionTable[it->first][it->second.second] = { it->second.first, ActionType::Shift };
					handledReduce.insert({ { it->first, it->second.second },  -(int)it->second.second});
				}
				else
					table.gotoTable[it->first][it->second.second - terminalEnd] = { it->second.first, false };
			}

			for (auto it = reduceItem.begin(); it != reduceItem.end(); ++it) {
				for (auto si = it->second.begin(); si != it->second.end(); ++si) {
					Item& item = closureVec[it->first][*si];
					ActionType type = ActionType::Reduce;
					if (grammerVec[item.prodId].first == acceptNonterminal)
						type = ActionType::Accept;

					std::vector<u64> lookahead = lookaheadSets[item.lookaheadId];
					for (u64 i = 0; i < lookahead.size(); i++) {
						u64 bit = lookahead[i];
						for (u64 j = 0; j < 64 && bit; j++) {
							if (bit & 0b1)
								table.actionTable[it->first][i * 64 + j] = { item.prodId, type };
							bit >>= 1;
						}
					}
				}
			}
		}

		void copyTable(ParserTable& dest, const ParserTable& src) {
			dest.size = src.size;
			dest.actionSize = src.actionSize;
			dest.gotoSize = src.gotoSize;

			dest.actionTable = new Action*[dest.size];
			dest.gotoTable = new Goto*[dest.size];

			for (size_t i = 0; i < dest.size; i++) {
				dest.actionTable[i] = new Action[dest.actionSize];
				dest.gotoTable[i] = new Goto[dest.gotoSize];

				for (size_t j = 0; j < dest.actionSize; j++)
					dest.actionTable[i][j] = src.actionTable[i][j];
				for (size_t j = 0; j < dest.gotoSize; j++)
					dest.gotoTable[i][j] = src.gotoTable[i][j];
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

		void printTable(ParserTable& table, int len) {
			std::cout << space(len + 4);
			for (size_t i = 0; i < table.actionSize; i++) {
				std::cout << "A" << int2str(i, len) << " ";
			}
			std::cout << "| ";
			for (size_t i = 0; i < table.gotoSize; i++) {
				std::cout << "G" << int2str(i, len) << " ";
			}
			std::cout << std::endl;

			for (size_t i = 0; i < table.size; i++) {
				std::cout << "ST" << int2str(i, len) << ": ";

				for (size_t j = 0; j < table.actionSize; j++) {
					Action& action = table.actionTable[i][j];
					switch (action.type) {
					case ActionType::Shift:
						std::cout << "S" << int2str(action.arg, len) << " ";
						break;
					case ActionType::Reduce:
						std::cout << "R" << int2str(action.arg, len) << " ";
						break;
					case ActionType::Accept:
						std::cout << "A" << int2str(action.arg, len) << " ";
						break;
					case ActionType::Error:
						std::cout << "E" << int2str(action.arg, len) << " ";
						break;
					}
				}

				std::cout << "| ";

				for (size_t j = 0; j < table.gotoSize; j++) {
					Goto& go = table.gotoTable[i][j];
					if (go.error)
						std::cout << "E" << int2str(go.state, len) << " ";
					else
						std::cout << "G" << int2str(go.state, len) << " ";
				}

				std::cout << std::endl;
			}
		}
	}

	typedef struct _PTNode {
		using Element = convert::Element;

		Element data;
		std::string text;
		struct _PTNode* parent;
		std::vector<struct _PTNode*> child;
	} PTNode;

	void delPT(PTNode* pPT) {
		std::vector<PTNode*> pPTvec;

		std::queue<PTNode*> searchQ;
		searchQ.push(pPT);

		while (!searchQ.empty()) {
			PTNode* cur = searchQ.front(); searchQ.pop();
			pPTvec.push_back(cur);
			for (auto it = cur->child.begin(); it != cur->child.end(); ++it)
				searchQ.push(*it);
		}

		for (auto it= pPTvec.rbegin(); it != pPTvec.rend(); ++it)
			delete (*it);
	}

	template <typename Terminal, typename NonTerminal, typename ASTElement>
	class ParserFactory;

	template <typename Terminal, typename NonTerminal, typename ASTElement>
	class Parser {
	private:
		friend ParserFactory<Terminal, NonTerminal, ASTElement>;

		using u64 = unsigned __int64;
		using ID = convert::ID;
		using Element = convert::Element;
		using Table = convert::ParserTable;

		std::vector<std::pair<Element, std::vector<Element>>> grammerVec;
		std::vector<std::pair<std::pair<Element, int>, std::vector<std::pair<bool, bool>>>> ASTActionVec;
		Table table;

	public:
		using Token = typename lexer_generator::Lexer<Terminal>::Token;

		typedef struct _ASTNode {
			Element type;
			std::string text;
			std::vector<struct _ASTNode*> child;

			_ASTNode(void) : type(0), text(""), child({}) {}
			_ASTNode(const Element& type, const std::string& text, const std::vector<struct _ASTNode*>& child) : type(type), text(text), child(child) {}
		} ASTNode;

		void delAST(ASTNode* pAST) {
			std::unordered_set<ASTNode*> pASTvec;

			std::queue<ASTNode*> searchQ;
			searchQ.push(pAST);
			while (!searchQ.empty()) {
				ASTNode* cur = searchQ.front(); searchQ.pop();
				pASTvec.insert(cur);
				for (auto it = cur->child.begin(); it != cur->child.end(); ++it)
					searchQ.push(*it);
			}

			for (auto it = pASTvec.begin(); it != pASTvec.end(); ++it) {
				delete (*it);
			}
		}

		bool parse(ASTNode*& pAST, std::vector<Token> inputs) {
			inputs.push_back({ "", Terminal::__eot});

			typedef struct _StackData {
				Element symbol;
				ID state;
			} StackData;

			std::stack<ASTNode*> astStack;
			std::stack<StackData> stack;
			stack.push({ (Element)0 , 0 });

			u64 tokenCount = 0;
			for (auto it = inputs.begin(); it != inputs.end();) {
				ID state = stack.top().state;

				using AT = convert::ActionType;

				auto action = this->table.actionTable[state][(Element)(it->type)];
				switch (action.type) {
					case AT::Shift:
						stack.push({ (Element)(it->type), action.arg });
						astStack.push(new ASTNode( (Element)it->type, it->text, {} ));
						++it;
						tokenCount++;
						break;
					case AT::Reduce: {
						//parseList.push_back(action.arg);
						auto& grammer = this->grammerVec[action.arg];
						auto& ASTRule = this->ASTActionVec[action.arg];

						ASTNode* nNode = new ASTNode;
						nNode->text = "";

						for (size_t i = grammer.second.size() - 1; true; i--) {
							stack.pop();
							ASTNode* cur = astStack.top(); astStack.pop();

							if (i == ASTRule.first.second) {
								nNode->text = cur->text;
								nNode->type = cur->type;
							}

							if (ASTRule.second[i].first) {
								if (ASTRule.second[i].second) {
									nNode->child.insert(nNode->child.begin(), cur->child.begin(), cur->child.end());
									delete cur;
								}
								else
									nNode->child.insert(nNode->child.begin(), cur);
							}
							else
								delAST(cur);

							if (!i)
								break;
						}
						if (ASTRule.first.first != -1)
							nNode->type = ASTRule.first.first;
						astStack.push(nNode);

						auto goData = this->table.gotoTable[stack.top().state][((u64)grammer.first - (u64)Terminal::__end)];
						if (goData.error)
							goto error;
						stack.push({ (Element)grammer.first, goData.state });
					}
						break;
					case AT::Accept:
						goto accept;
					case AT::Error:
						goto error;
					default:
						goto error;
				}
			}

		accept:
			pAST = astStack.top();
			return true;

		error:
			ASTNode* nNode = new ASTNode;
			ASTNode* errNode = new ASTNode;
			for (u64 i = 0; i < tokenCount; i++)
				errNode->text += inputs[i].text + " ";
			nNode->child.push_back(errNode);
			while (!astStack.empty()) {
				ASTNode* cur = astStack.top(); astStack.pop();
				nNode->child.push_back(cur);
			}
			pAST = nNode;
			return false;
		}
	};

	template <typename Terminal, typename NonTerminal, typename ASTElement>
	class ParserFactory {
	private:
		using u64 = unsigned __int64;
		using ID = convert::ID;
		using Element = convert::Element;
		using Table = convert::ParserTable;

	public:
		class ElementWrapper {
		public:
			enum class Type {
				RAW,
				TERMINAL,
				NONTERMINAL,
				ASTTYPE
			};

		private:
			Type type;
			union {
				Terminal terminal;
				NonTerminal nonTerminal;
				ASTElement astElement;
				u64 raw;
			};

		public:
			ElementWrapper(void) : type(Type::RAW), terminal(Terminal(0)) {}
			ElementWrapper(Terminal t) : type(Type::TERMINAL), terminal(t) {}
			ElementWrapper(NonTerminal nt) : type(Type::NONTERMINAL), nonTerminal(nt) {}
			ElementWrapper(ASTElement at) : type(Type::ASTTYPE), astElement(at) {}

			const ElementWrapper& operator = (const ElementWrapper& other) {
				this->type = other.type;
				this->raw = other.raw;
				return *this;
			}

			inline convert::Element get(void) const {
				switch (this->type) {
				case Type::TERMINAL:
					return (convert::Element)this->terminal;
				case Type::NONTERMINAL:
					return (convert::Element)this->nonTerminal + (convert::Element)Terminal::__end;
				case Type::ASTTYPE:
					if ((Element)this->astElement == -1)
						return -1;
					return (convert::Element)this->astElement + (convert::Element)Terminal::__end;
				}
				return (convert::Element)this->raw;
			}

			inline std::pair<Type, u64> getRaw(void) const {
				return { this->type, (u64)this->raw };
			}
		};

		typedef struct _Head {
			ElementWrapper symbol;
			ElementWrapper nodeType;
			int nodeIndex;
		} Head;

		typedef struct _Tail {
			ElementWrapper symbol;
			bool useNode, expand;
		} Tail;

		using CreateData = std::vector<std::pair<Head, std::vector<Tail>>>;

		typedef struct _UpdateData {
			std::vector<std::pair<Element, std::vector<Element>>> grammerVec;
			std::vector<std::pair<std::pair<Element, int>, std::vector<std::pair<bool, bool>>>>ASTActionVec;
			Table table;
		} UpdateData;

	private:
		std::vector<std::pair<Element, std::vector<Element>>> grammerVec;
		std::vector<std::pair<std::pair<Element, int>, std::vector<std::pair<bool, bool>>>> ASTActionVec;
		Table table;

		void clearTable(void) {
			if (this->table.actionTable != NULL) {
				for (size_t i = 0; i < this->table.size; i++)
					delete[] this->table.actionTable[i];
				delete[] this->table.actionTable;
			}
			if (this->table.gotoTable != NULL) {
				for (size_t i = 0; i < this->table.size; i++)
					delete[] this->table.gotoTable[i];
				delete[] this->table.gotoTable;
			}
			this->table.size = 0;
			this->table.actionSize = 0;
			this->table.gotoSize = 0;
			this->table.actionTable = NULL;
			this->table.gotoTable = NULL;
		}

	public:
		ParserFactory(void) {
			this->grammerVec.clear();
			this->table.actionTable = NULL;
			this->table.gotoTable = NULL;
			this->table.size = 0;
		}

		~ParserFactory() {
			this->grammerVec.clear();
			this->clearTable();
		}

		void setRules(const CreateData& data) {
			for (auto it = data.cbegin(); it != data.cend(); ++it) {
				std::vector<Element> rhsG;
				std::vector<std::pair<bool, bool>> rhsA;
				for (auto si = it->second.cbegin(); si != it->second.cend(); ++si) {
					rhsG.push_back(si->symbol.get());
					rhsA.push_back({ si->useNode, si->expand });
				}
				grammerVec.push_back({ it->first.symbol.get(), rhsG });
				ASTActionVec.push_back({ { it->first.nodeType.get(), it->first.nodeIndex }, rhsA });
			}
		}

		void update(void) {
			this->clearTable();

			convert::createTable(this->table, this->grammerVec, (Element)Terminal::__end, (Element)NonTerminal::__end, (Element)NonTerminal::__accept);
		}

		void getData(UpdateData& out) {
			out.ASTActionVec = this->ASTActionVec;
			out.grammerVec = this->grammerVec;
			convert::copyTable(out.table, this->table);
		}

		void directUpdate(const UpdateData& in) {
			this->ASTActionVec = in.ASTActionVec;
			this->grammerVec = in.grammerVec;
			convert::copyTable(this->table, in.table);
		}

		Parser<Terminal, NonTerminal, ASTElement> create(void) {
			Parser<Terminal, NonTerminal, ASTElement> parser;
			parser.table = this->table;
			parser.grammerVec = this->grammerVec;
			parser.ASTActionVec = this->ASTActionVec;
			return parser;
		}
	};
}

#endif