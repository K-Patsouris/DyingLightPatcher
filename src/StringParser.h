#pragma once
#include "Common.h"
#include "Containers.h"
#include "AssosciativeCache.h"
#include "Types.h"
#include "Utils.h"

#include <mutex>


namespace StringParser {

	class Parser {
	public:
		using Cache = AssosciativeCache<string, uint32>;

		struct Node final {
		public:
			using NodeVector = vector<Node>;
			using NodeIterator = NodeVector::iterator;
			using NodeCIterator = NodeVector::const_iterator;

			enum Flag : uint32 {
				//Flag flags
				Noop = 0,	//Handle subnodes only
				Insert,		//Append this node and its subnodes to the front of its parent target. Subnode operation members are ignored.
				Rename,		//Change this signature to the signature of the first subnode. First subnode's operation and other subnodes are ignored. Traversing continues from first subnode.
				Redefine,	//Overwrite target contents with this node's subnodes. Subnode operation members are ignored.
				Delete,		//Delete contents of target. Subnodes are ignored.

				//Type flags
				Import,
				Export,
				SubScope,
				SubDeclaration,
				Use,
				Function,
				Include,
				Vardecl,
			};
			using NodeFlags = BitFlagsRaw<Flag>;


			Node(uint32 sID, uint32 nsID, uint32 cmpID, NodeFlags op, uint32 bkID, uint64 srcln) noexcept;
			Node() = default;
			Node(const Node&) = default;
			Node(Node&& rhs) noexcept = default;
			Node& operator=(const Node&)  = default;
			Node& operator=(Node&& rhs) noexcept = default;
			~Node() = default;


			[[nodiscard]] Node& AddAndReturnSubnode(uint32 sID, uint32 nsID, uint32 cmpID, NodeFlags op, uint32 bkID, uint64 srcln) noexcept;
			[[nodiscard]] Node& AddAndReturnSubnode(const Node& node) noexcept;
			[[nodiscard]] Node& AddAndReturnSubnode(Node&& node) noexcept;
			bool AddSubnode(uint32 sID, uint32 nsID, uint32 cmpID, NodeFlags op, uint32 bkID, uint64 srcln) noexcept;
			bool AddSubnode(const Node& node) noexcept;
			bool AddSubnode(Node&& node) noexcept;

			bool DeleteSubnode(const uint32 id) noexcept;
			bool RenameSubnode(const uint32 id, const uint32 newid) noexcept;

			void SetSigID(const uint32 newid) noexcept;
			void SetNewsigID(const uint32 newnewid) noexcept;
			void SetComparesigID(const uint32 newcompid) noexcept;
			bool SetSubnodes(const NodeVector& newsubnodes) noexcept;
			void SetFlags(const NodeFlags newflags) noexcept;
			void SetOrder(const uint32 neworder) noexcept;
			void SetOrderSigID(const uint32 newid) noexcept;
			void SetSourceLine(const uint64 newsourceline) noexcept;

			bool SegregateAndOrderSubnodes(const NodeVector& rhs, const Cache& string_cache) noexcept;

			[[nodiscard]] uint32 GetSigID() const noexcept;
			[[nodiscard]] uint32 GetNewsigID() const noexcept;
			[[nodiscard]] uint32 GetComparesigID() const noexcept;
			[[nodiscard]] const NodeVector& GetSubnodes() const noexcept;
			[[nodiscard]] szt GetNumSubnodes() const noexcept;
			[[nodiscard]] NodeFlags GetFlags() const noexcept;
			[[nodiscard]] uint32 GetOrder() const noexcept;
			[[nodiscard]] uint32 GetOrdersigID() const noexcept;
			[[nodiscard]] uint64 GetSourceLine() const noexcept;

			[[nodiscard]] NodeVector CopySubnodes() const;
			[[nodiscard]] const NodeVector& GetSubnodesRef() const noexcept;

			template<typename... Flags> requires(sizeof...(Flags) > 0 and (std::same_as<Flags, Flag> and ...))
			void Set(Flags... vals) noexcept { return flags.Set(vals...); }
			template<typename... Flags> requires(sizeof...(Flags) > 0 and (std::same_as<Flags, Flag> and ...))
			void Unset(Flags... vals) noexcept { return flags.Unset(vals...); }
			template<typename... Flags> requires(sizeof...(Flags) > 0 and (std::same_as<Flags, Flag> and ...))
			[[nodiscard]] bool Any(Flags... vals) const noexcept { return flags.Any(vals...); }
			template<typename... Flags> requires(sizeof...(Flags) > 0 and (std::same_as<Flags, Flag> and ...))
			[[nodiscard]] bool All(Flags... vals) const noexcept { return flags.All(vals...); }
			[[nodiscard]] bool Only(Flag val) const noexcept { return flags.Only(val); }
			template<typename... Flags> requires(sizeof...(Flags) > 0 and (std::same_as<Flags, Flag> and ...))
			[[nodiscard]] bool Only(Flags... vals) const noexcept { return flags.Only(vals...); }
			[[nodiscard]] bool None() const noexcept { return flags.None(); }

			[[nodiscard]] NodeIterator Begin() noexcept;
			[[nodiscard]] NodeCIterator CBegin() const noexcept;
			[[nodiscard]] NodeIterator End() noexcept;
			[[nodiscard]] NodeCIterator CEnd() const noexcept;
			[[nodiscard]] NodeIterator Find(const uint32 id) noexcept;
			[[nodiscard]] NodeIterator Find(const vector<uint32>& idtree) noexcept;
			[[nodiscard]] NodeIterator Find(vector<uint32>::const_iterator start, const vector<uint32>::const_iterator end) noexcept;


			[[nodiscard]] string ToString(szt depth, const Cache& string_cache) const;
			[[nodiscard]] string ToStringAttr(szt depth, const Cache& string_cache) const;

		private:
			uint32 sigID{ Cache::NULL_ID };			//00
			uint32 newsigID{ Cache::NULL_ID };		//04
			uint32 comparesigID{ Cache::NULL_ID };	//
			NodeFlags flags{};						//
			uint32 ordersigID;						//
			uint32 order{ 0 };						//
			uint64 sourceline{ 0u };				//
			NodeVector subnodes{};					//
		};
		static_assert(sizeof(Node) == 56u);
		


		[[nodiscard]] bool SetDiff(const string& diff_str) noexcept;
		[[nodiscard]] bool SetTarget(const string& diff_str) noexcept;
		[[nodiscard]] string GetTargetPath() const;

		[[nodiscard]] bool Parse(string& out) noexcept;


		void Reset() noexcept;

		void PrintTrees() const;


	private:
		enum class FileType {
			scr = 0,
			def,
			loot, 
			varlist,

			INVALID_FILETYPE
		};

		using Locker = std::lock_guard<std::mutex>;
		mutable std::mutex lock{};
		vector<Node> diff{};
		vector<Node> target{};
		string target_path{};
		FileType filetype{ FileType::INVALID_FILETYPE };
		Cache string_cache{};

		[[nodiscard]] bool SetFile(const string& str, bool isdiff);
		[[nodiscard]] bool DeduceFileInfo(const string& firstline);

		//Tree generating
		[[nodiscard]] bool GenerateTreeScr(const string& str, bool isdiff);
		[[nodiscard]] bool GenerateTreeDef(const string& str, bool isdiff);
		[[nodiscard]] bool GenerateTreeLoot(const string& str, bool isdiff);
		[[nodiscard]] bool GenerateTreeVarlist(const string& str, bool isdiff);
		[[nodiscard]] bool GenerateImportNodes(const string& str, StringUtils::traversal_state& ts, bool isdiff) noexcept;
		[[nodiscard]] bool GenerateExportNodes(const string& str, StringUtils::traversal_state& ts, bool isdiff) noexcept;
		[[nodiscard]] bool GenerateVarlistNodes(const string& str, StringUtils::traversal_state& ts, bool isdiff) noexcept;
		[[nodiscard]] bool GenerateScopeNodes(Node& parent_node, const string& str, StringUtils::traversal_state& ts, bool isdiff) noexcept;
		[[nodiscard]] bool IdentifyAttribute(const string& str, Node::Flag& out) const noexcept;
		[[nodiscard]] bool ParseAttributes(const string& str, StringUtils::traversal_state& ts, Node::NodeFlags& out) const noexcept;

		//Tree parsing
		[[nodiscard]] bool ParseScrLoot(string& out);
		[[nodiscard]] bool ParseDef(string& out);
		[[nodiscard]] bool ParseVarlist(string& out);
		[[nodiscard]] bool ParseNode(const Node& dNode, const Node& tNode, Node& rNode);

		[[nodiscard]] vector<Node>& GetVec(bool isdiff) noexcept;
		[[nodiscard]] const string& CacheFind(uint32 id) const noexcept;
		[[nodiscard]] const string& CacheFindSig(const Node& node) const noexcept;

		void ResetImpl() noexcept;
		void HandleResets(bool isdiff) noexcept;
		
	};
	//static_assert(sizeof(Parser) == 168u);


}



