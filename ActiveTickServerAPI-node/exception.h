namespace ActiveTickServerAPI_node {

	class queue_overflow : public std::bad_alloc {
	public:
		const char* what() const { return "queue overflow"; }
	};

	class bad_data : public std::runtime_error {
	public:
		bad_data() : std::runtime_error("bad data") {}
	};
}