
namespace ActiveTickServerAPI_node {
	using namespace v8;

	struct Message {
		enum Type {
			None,
			SessionStatusChange,
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
			char buffer[17];
			v8set(value, "session", _ui64toa(session, buffer, 16));
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

}