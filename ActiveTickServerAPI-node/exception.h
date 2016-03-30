namespace ActiveTickServerAPI_node {

	class queue_overflow : public std::bad_alloc {
	public:
		const char* what() const { return "queue overflow"; }
	};

	class bad_data : public std::exception {
	public:
		bad_data() : std::exception("bad data", 1) {}
	};

	class failure : public std::exception {
	public:
		failure(ATTickHistoryResponseType responseType) : std::exception(convert(responseType), 1) {}
		failure(ATSymbolStatus symbolStatus) : std::exception(convert(symbolStatus), 1) {}

	private:
		static const char* convert(ATTickHistoryResponseType responseType) {
			switch (responseType) {
				case TickHistoryResponseSuccess:
					return "tick-history-response-success";
				case TickHistoryResponseInvalidRequest:
					return "tick-history-response-invalid-request";
				case TickHistoryResponseMaxLimitReached:
					return "tick-history-response-max-limit-reached";
				case TickHistoryResponseDenied:
					return "tick-history-response-denied";
			}
			return "tick-history-response-unknown";
		}

		static const char* convert(ATSymbolStatus symbolStatus) {
			switch (symbolStatus) {
				case SymbolStatusSuccess:
					return "symbol-status-success";
				case SymbolStatusInvalid:
					return "symbol-status-invalid";
				case SymbolStatusUnavailable:
					return "symbol-status-unavailable";
				case SymbolStatusNoPermission:
					return "symbol-status-no-permission";
			}
			return "symbol-status-unknown";
		}
	};
}