#include <time.h>

namespace ActiveTickServerAPI_node {
	using namespace v8;

	struct Message {
		enum Type {
			None,
			SessionStatusChange,
			RequestTimeout,
			LoginResponse,
			QuoteStreamResponse,
			QuoteStreamSymbol,
			StreamUpdateTrade,
			StreamUpdateQuote,
			StreamUpdateRefresh,
			StreamUpdateTopMarketMovers,
		};

		Type type;
		uint64_t session;
		uint64_t request;

		Message() : type(None), session(0), request(0) {}

		virtual ~Message() {}

		static void* operator new(size_t size, Queue &q) {
			return q.allocate(size);
		}

		static void operator delete(void* p, Queue &q) {
			q.release(p);
		}

		virtual Handle<Value> value() {
			auto value = Object::New();
			v8set(value, "message", name(type));
			populate(value);
			if (request)
				v8set(value, "request", session, request);
			else if (session)
				v8set(value, "session", session);
			return value;
		}

	protected:
		Message(Type type, uint64_t session = 0, uint64_t request = 0) : 
			type(type),
			session(session),
			request(request)
		{}

		virtual void populate(Handle<Object> value) {}

		static double convert(const ATTIME& time) {
			tm t{
				time.second,
				time.minute,
				time.hour,
				time.day,
				time.month - 1,
				time.year - 1900,
				0, 0, -1
			};
			auto seconds = mktime(&t);
			return (double)seconds * 1000.0 + time.milliseconds;
		}

		static double convert(const ATPRICE& price) {
			return price.price;
		}

	private:
		static const char* name(Type type) {
			switch (type) {
				case None:
					return "";
				case SessionStatusChange:
					return "session-status-change";
				case RequestTimeout:
					return "request-timeout";
				case LoginResponse:
					return "login-response";
				case QuoteStreamResponse:
					return "quote-stream-response";
				case QuoteStreamSymbol:
					return "quote-stream-symbol";
				case StreamUpdateTrade:
					return "stream-update-trade";
				case StreamUpdateQuote:
					return "stream-update-quote";
				case StreamUpdateRefresh:
					return "stream-update-refresh";
				case StreamUpdateTopMarketMovers:
					return "stream-update-top-market-movers";
			}
			return "unknown";
		}
	};

	struct SessionStatusChangeMessage : Message {
		ATSessionStatusType statusType;

		SessionStatusChangeMessage(uint64_t session, ATSessionStatusType statusType) :
			Message(SessionStatusChange, session),
			statusType(statusType)
		{}

		void populate(Handle<Object> value) {
			v8set(value, "sessionStatus", convert(statusType));
		}

		static const char* convert(ATSessionStatusType status) {
			switch (status) {
				case SessionStatusDisconnected:
					return "disconnected";
				case SessionStatusDisconnectedDuplicateLogin:
					return "disconnected-duplicate-login";
				case SessionStatusConnected:
					return "connected";
			}
			return "unknown";
		}
	};

	struct RequestTimeoutMessage : Message {
		RequestTimeoutMessage(uint64_t session, uint64_t request) :
			Message(RequestTimeout, session, request)
		{}
	};

	struct LoginResponseMessage : Message {
		ATLOGIN_RESPONSE response;

		LoginResponseMessage(uint64_t session, uint64_t request, ATLOGIN_RESPONSE& response) :
			Message(LoginResponse, session, request),
			response(response)
		{}

		void populate(Handle<Object> value) {
			v8set(value, "loginResponse", convert(response.loginResponse));
			//v8set(value, "permissions", permissions());
			v8set(value, "serverTime", Message::convert(response.serverTime));
		}

		static const char* convert(ATLoginResponseType response) {
			switch (response) {
				case LoginResponseSuccess:
					return "success";
				case LoginResponseInvalidUserid:
					return "invalid-userid";
				case LoginResponseInvalidPassword:
					return "invalid-password";
				case LoginResponseInvalidRequest:
					return "invalid-request";
				case LoginResponseLoginDenied:
					return "login-denied";
				case LoginResponseServerError:
					return "server-error";
			}
			return "unknown";
		}
	};

	struct QuoteStreamResponseMessage : Message {
		ATQUOTESTREAM_RESPONSE response;

		QuoteStreamResponseMessage(uint64_t session, uint64_t request, ATQUOTESTREAM_RESPONSE& response) :
			Message(QuoteStreamResponse, session, request),
			response(response)
		{}

		void populate(Handle<Object> value) {
			v8set(value, "streamResponse", convert(response.responseType));
			v8set(value, "count", response.dataItemCount);
		}

		static const char* convert(ATStreamResponseType response) {
			switch (response) {
				case StreamResponseSuccess:
					return "success";
				case StreamResponseInvalidRequest:
					return "invalid-request";
				case StreamResponseDenied:
					return "denied";
			}
			return "unknown";
		}
	};

	struct QuoteStreamSymbolResponseMessage : Message {
		ATQUOTESTREAM_DATA_ITEM item;
		int index;

		QuoteStreamSymbolResponseMessage(uint64_t session, uint64_t request, ATQUOTESTREAM_DATA_ITEM& item, int index) :
			Message(QuoteStreamSymbol, session, request),
			item(item),
			index(index)
		{}

		void populate(Handle<Object> value) {
			v8set(value, "symbol", item.symbol.symbol);
			v8set(value, "symbolStatus", convert(item.symbolStatus));
			v8set(value, "index", index);
		}

		static const char* convert(ATSymbolStatus symbolStatus) {
			switch (symbolStatus) {
				case SymbolStatusSuccess:
					return "success";
				case SymbolStatusInvalid:
					return "invalid";
				case SymbolStatusUnavailable:
					return "unavailable";
				case SymbolStatusNoPermission:
					return "no-permission";
			}
			return "unknown";
		}
	};
}