#include <time.h>

namespace ActiveTickServerAPI_node {
	using namespace v8;

	struct Message {
		enum Type {
			None,
			SessionStatusChange,
			RequestTimeout,
			LoginResponse,
		};

		Type type;

		Message() : type(None) {}

		virtual ~Message() {}

		static void* operator new(size_t size, Queue q) {
			return q.alloc(size);
		}

		static void operator delete(void* p, Queue q) {
			q.free(p);
		}

		const char* name() {
			return _names[type];
		}

		virtual Handle<Value> value() {
			return Object::New();
		}

	protected:
		Message(Type type) : type(type) {}

	private:
		static const char * const _names[];
	};

	const char * const Message::_names[] = {
		"",
		"session-status-change",
		"request-timeout",
		"login-response",
	};

	struct SessionStatusChangeMessage : Message {
		uint64_t session;
		ATSessionStatusType statusType;

		SessionStatusChangeMessage(uint64_t session, ATSessionStatusType statusType) :
			Message(SessionStatusChange),
			session(session),
			statusType(statusType)
		{}

		Handle<Value> value() {
			auto value = Object::New();
			v8set(value, "session", session);
			v8set(value, "status", status());
			return value;
		}

		const char* status() {
			switch (statusType) {
				case SessionStatusDisconnected:
					return "disconnected";
				case SessionStatusDisconnectedDuplicateLogin:
					return "disconnected (duplicate login)";
				case SessionStatusConnected:
					return "connected";
			}
			return "unknown";
		}
	};

	struct RequestTimeoutMessage : Message {
		uint64_t request;

		RequestTimeoutMessage(uint64_t request) :
			Message(RequestTimeout),
			request(request)
		{}

		Handle<Value> value() {
			auto value = Object::New();
			v8set(value, "request", request);
			return value;
		}
	};

	struct LoginResponseMessage : Message {
		uint64_t session;
		uint64_t request;
		ATLOGIN_RESPONSE response;

		LoginResponseMessage(uint64_t session, uint64_t request, LPATLOGIN_RESPONSE response) :
			Message(LoginResponse),
			session(session),
			request(request),
			response(*response)
		{}

		Handle<Value> value() {
			auto value = Object::New();
			v8set(value, "session", session);
			v8set(value, "request", request);
			v8set(value, "loginResponse", loginResponse());
			//v8set(value, "permissions", permissions());
			v8set(value, "serverTime", serverTime());
			return value;
		}

		const char* loginResponse() {
			switch (response.loginResponse) {
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

		double serverTime() {
			tm t {
				response.serverTime.second,
				response.serverTime.minute,
				response.serverTime.hour,
				response.serverTime.day,
				response.serverTime.month,
				response.serverTime.year,
				0, 0, -1
			};
			auto seconds = mktime(&t);
			return (double)seconds * 1000.0 + response.serverTime.milliseconds;
		}
	};
}