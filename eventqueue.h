
namespace ActiveTickServerAPI_node {

	class EventQueue {
		const void* _;
	public:
		void* alloc(size_t size) { return ::malloc(size); }
		void push(const void* p) { _ = p; }
		const void* pop() { return _; }
		void free(void* p) { ::free(p); }
	};

}