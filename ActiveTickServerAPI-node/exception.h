namespace ActiveTickServerAPI_node {

	class exception : public std::exception {
	public:
		exception(const char* msg) : std::exception(msg, 1) {}
	};

}