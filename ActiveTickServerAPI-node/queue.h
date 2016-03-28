#include <atomic>
#include <chrono>
#include <thread>

#define roundup(value, pow2) ((value + pow2 - 1) & ~(pow2 - 1))
#define mod(value, pow2) ((value) & ((pow2) - 1))
#define isPowerOf2(x) (((x) & ((x)-1)) == 0)

namespace ActiveTickServerAPI_node {

	class Queue {
		Queue(const Queue&) = delete;
		Queue& operator=(const Queue&) = delete;

		class Indicator {
			size_t _value;

			static const size_t Mask = SIZE_MAX >> 2;
			static const size_t Committed = 0x2 << (sizeof(size_t) * 8 - 2);
			static const size_t Failed = 0x3 << (sizeof(size_t) * 8 - 2);

		public:
			inline Indicator() {}
			inline explicit Indicator(size_t size) : _value(size) {}

			inline size_t size() const {
				return _value & Mask;
			}

			inline bool free() const {
				return (_value & ~Mask) == 0;
			}

			inline Indicator commit() const {
				return Indicator((_value & Mask) | Committed);
			}

			inline bool committed() const {
				return (_value & ~Mask) == Committed;
			}

			inline Indicator fail() const {
				return Indicator((_value & Mask) | Failed);
			}

			inline bool failed() const {
				return (_value & ~Mask) == Failed;
			}
		};

		class Header {
			std::atomic<Indicator> _value;

		public:
			static inline Header* At(const void* p, size_t offset) {
				return (Header*)((const char*)p + offset);
			}

			inline Header* preceding() const {
				return (Header*)((const char*)this - BlockSize);
			}

			inline void* payload() const {
				return (void*)((const char*)this + HeaderSize);
			}

			static inline Header* Of(const void* payload) {
				return (Header*)((const char*)payload - HeaderSize);
			}
			
			inline void set(Indicator value) {
				_value.store(value, std::memory_order_relaxed);
			}

			inline Indicator get() const {
				return _value.load(std::memory_order_relaxed);
			}

			inline void release(Indicator value) {
				_value.store(value, std::memory_order_release);
			}

			inline Indicator acquire() const {
				return _value.load(std::memory_order_acquire);
			}
		};

		class Place {
			Place(const Place&) = delete;
			Place& operator=(const Place&) = delete;
			std::atomic<size_t> _value;

		public:
			inline Place() {
				_value.store(0, std::memory_order_relaxed);
			}

			inline operator size_t() const {
				return _value.load(std::memory_order_relaxed);
			}

			inline bool try_advance(size_t& current, size_t amount, size_t BufferSize) {
				auto next = mod(current + amount, BufferSize);
				return _value.compare_exchange_weak(current, next, std::memory_order_acq_rel, std::memory_order_relaxed);
			}

			inline void spin_advance(size_t expected, size_t amount, size_t BufferSize) {
				auto next = mod(expected + amount, BufferSize);
				size_t current;
				do {
					current = expected;
				} while (!_value.compare_exchange_weak(current, next, std::memory_order_acq_rel, std::memory_order_relaxed));
			}
		};

		static void zero(void* p, size_t outersize) {
			auto header = Header::At(p, outersize);
			while (header > p) {
				header = header->preceding();
				header->set(Indicator(0));
			}
		}

		void _release(Header* header, size_t size) {
			zero(header, size);
			_trailing.spin_advance(placeOf(header), size, BufferSize);
		}

		// Header is an allocation's preamble
		// We round up the size for memory alignment
		static const size_t HeaderSize = roundup(sizeof(Header), __alignof(std::max_align_t));

		// Blocks are the granularity of allocation.  
		// Needs to be a power-of-2, so a whole number of them fit in the buffer
		static const size_t BlockSize = HeaderSize;
		
		size_t BufferSize;
		void* _buffer;
		Place _head;
		Place _tail;
		Place _trailing;

		inline Header* headerAt(size_t offset) {
			return Header::At(_buffer, offset);
		}

		inline size_t placeOf(Header* header) {
			return (char*)header - (char*)_buffer;
		}

		inline size_t bytesBetween(size_t start, size_t end) {
			auto bytes = BufferSize - mod(start - end, BufferSize);
			return bytes;
		}

		inline size_t bytesToEnd(size_t head) {
			return bytesBetween(head, 0);
		}

		Header* _claim(size_t size) {
			size_t head = _head, remaining, claim;
			do {
				// check for wrapping end of buffer
				remaining = bytesToEnd(head);
				claim = (size <= remaining) ? size : size + remaining;

				// check for lapping
				if (claim >= bytesBetween(head, _trailing))
					return NULL;
			} while (!_head.try_advance(head, claim, BufferSize));

			// if we wrapped the buffer, then mark the 'remaining' released
			auto header = headerAt(head);
			if (claim > size) {
				header->release(Indicator(remaining).fail());
				header = headerAt(0);
			}

			return header;
		}

		Header* claim(size_t size) {
			Header* header;
			int tries = 100;
			while (!(header = _claim(size)) && tries--)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			if (!header)
				throw queue_overflow();
			return header;
		}

	public:
		Queue(size_t size) : BufferSize(size) {
			assert(isPowerOf2(BlockSize));
			assert(isPowerOf2(BufferSize));
			_buffer = ::malloc(BufferSize);
			zero(_buffer, BufferSize);
		}

		~Queue() {
			::free(_buffer);
		}

		void* allocate(size_t size) {
			// allocate more than requested so we can place a header in front
			size = roundup(HeaderSize + size, BlockSize);

			// lay claim to some memory
			auto header = claim(size);

			// we now have exclusive write-access to our memory
			// although others can read our header

			header->release(Indicator(size));

			// return the memory after our header
			return header->payload();
		}

		void push(void* p) { 
			// we have exclusive write-access to our memory
			auto header = Header::Of(p);
			auto indicator = header->get();
			header->release(indicator.commit());
		}

		template<typename T> T* pop() { 
			for(;;) {
				Header* header;
				Indicator indicator;

				// lay claim to the entry pointed to by _tail
				size_t tail = _tail;
				do {
					header = headerAt(tail);
					indicator = header->acquire();
					// if it's Free, then there's nothing to pop
					if (indicator.free())
						return NULL;
				} while (!_tail.try_advance(tail, indicator.size(), BufferSize));

				// now we have exclusive write access
				// although others can read our header
				if (indicator.committed())
					return (T*)(header->payload());

				// skip failed entries
				_release(header, indicator.size());
			}
		}

		void release(void* p) { 
			// we have exclusive write-access to our memory
			auto header = Header::Of(p);
			auto indicator = header->get();
			_release(header, indicator.size());
		}
	};

}

#undef isPowerOf2
#undef mod
#undef roundup
