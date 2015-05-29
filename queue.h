
namespace ActiveTickServerAPI_node {

	class Queue {
		void* _;
	public:
		void* alloc(size_t size) { return ::malloc(size); }
		void push(void* p) { _ = p; }
		template<typename T> T* pop() { return (T*)_; }
		void free(void* p) { ::free(p); }
	};

}