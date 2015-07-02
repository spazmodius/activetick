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

	struct StreamUpdateTradeMessage : Message {
		ATQUOTESTREAM_TRADE_UPDATE trade;

		StreamUpdateTradeMessage(ATQUOTESTREAM_TRADE_UPDATE& trade) :
			Message(StreamUpdateTrade),
			trade(trade)
		{}

		void populate(Handle<Object> value) {
			v8set(value, "symbol", trade.symbol.symbol);
			v8set(value, "flags", convert(trade.flags));
			v8set(value, "conditions", convert(trade.condition));
			v8set(value, "exchange", convert(trade.lastExchange));
			v8set(value, "price", Message::convert(trade.lastPrice));
			v8set(value, "size", trade.lastSize);
			v8set(value, "time", Message::convert(trade.lastDateTime));
		}

		static Handle<Object> convert(ATTradeMessageFlags flags) {
			auto value = Object::New();
			if (flags & TradeMessageFlagRegularMarketLastPrice)
				v8set(value, "regularMarketLastPrice", True());
			if (flags & TradeMessageFlagRegularMarketVolume)
				v8set(value, "regularMarketVolume", True());
			if (flags & TradeMessageFlagHighPrice)
				v8set(value, "highPrice", True());
			if (flags & TradeMessageFlagLowPrice)
				v8set(value, "lowPrice", True());
			if (flags & TradeMessageFlagDayHighPrice)
				v8set(value, "dayHighPrice", True());
			if (flags & TradeMessageFlagDayLowPrice)
				v8set(value, "dayLowPrice", True());
			if (flags & TradeMessageFlagExtendedMarketLastPrice)
				v8set(value, "extendedMarketLastPrice", True());
			if (flags & TradeMessageFlagPreMarketVolume)
				v8set(value, "preMarketVolume", True());
			if (flags & TradeMessageFlagAfterMarketVolume)
				v8set(value, "afterMarketVolume", True());
			if (flags & TradeMessageFlagPreMarketOpenPrice)
				v8set(value, "preMarketOpenPrice", True());
			if (flags & TradeMessageFlagOpenPrice)
				v8set(value, "openPrice", True());
			return value;
		}

		static Handle<Object> convert(const ATTradeConditionType (&conditions)[ATTradeConditionsCount]) {
			auto value = Object::New();
			for (int i = 0; i < ATTradeConditionsCount; ++i) {
				auto condition = convert(conditions[i]);
				if (condition)
					v8set(value, condition, True());
			}
			return value;
		}

		static const char* convert(ATTradeConditionType condition) {
			switch (condition) {
				//case TradeConditionRegular:
					//return "Regular";
				case TradeConditionAcquisition:
					return "acquisition";
				case TradeConditionAveragePrice:
					return "averagePrice";
				case TradeConditionAutomaticExecution:
					return "automaticExecution";
				case TradeConditionBunched:
					return "bunched";
				case TradeConditionBunchSold:
					return "bunchSold";
				case TradeConditionCAPElection:
					return "capElection";
				case TradeConditionCash:
					return "cash";
				case TradeConditionClosing:
					return "closing";
				case TradeConditionCross:
					return "cross";
				case TradeConditionDerivativelyPriced:
					return "derivativelyPriced";
				case TradeConditionDistribution:
					return "distribution";
				case TradeConditionFormT:
					return "formT";
				case TradeConditionFormTOutOfSequence:
					return "formTOutOfSequence";
				case TradeConditionInterMarketSweep:
					return "interMarketSweep";
				case TradeConditionMarketCenterOfficialClose:
					return "marketCenterOfficialClose";
				case TradeConditionMarketCenterOfficialOpen:
					return "marketCenterOfficialOpen";
				case TradeConditionMarketCenterOpening:
					return "marketCenterOpening";
				case TradeConditionMarketCenterReOpenning:
					return "marketCenterReOpenning";
				case TradeConditionMarketCenterClosing:
					return "marketCenterClosing";
				case TradeConditionNextDay:
					return "nextDay";
				case TradeConditionPriceVariation:
					return "priceVariation";
				case TradeConditionPriorReferencePrice:
					return "priorReferencePrice";
				case TradeConditionRule155Amex:
					return "rule155Amex";
				case TradeConditionRule127Nyse:
					return "rule127Nyse";
				case TradeConditionOpening:
					return "opening";
				case TradeConditionOpened:
					return "opened";
				case TradeConditionRegularStoppedStock:
					return "regularStoppedStock";
				case TradeConditionReOpening:
					return "reOpening";
				case TradeConditionSeller:
					return "seller";
				case TradeConditionSoldLast:
					return "soldLast";
				case TradeConditionSoldLastStoppedStock:
					return "soldLastStoppedStock";
				case TradeConditionSoldOutOfSequence:
					return "soldOutOfSequence";
				case TradeConditionSoldOutOfSequenceStoppedStock:
					return "soldOutOfSequenceStoppedStock";
				case TradeConditionSplit:
					return "split";
				case TradeConditionStockOption:
					return "stockOption";
				case TradeConditionYellowFlag:
					return "yellowFlag";
			}
			return NULL;
		}

		static const char* convert(ATExchangeType exchange/*, ATSymbolType symbolType, ATCountryType country*/) {
			switch (exchange) {
				case ExchangeAMEX:
					return "AMEX";
				case ExchangeNasdaqOmxBx:
				//case ExchangeOptionBoston:
					//if (symbolType == SymbolStockOption)
						//return "OptionBoston";
					return "NasdaqOmxBx";
				case ExchangeNationalStockExchange:
				//case ExchangeOptionCboe:
					//if (symbolType == SymbolStockOption)
						//return "OptionCboe";
					return "NationalStockExchange";
				case ExchangeFinraAdf:
					return "FinraAdf";
				case ExchangeCQS:
					return "CQS";
				case ExchangeForex:
					return "Forex";
				case ExchangeInternationalSecuritiesExchange:
					return "InternationalSecuritiesExchange";
				case ExchangeEdgaExchange:
					return "EdgaExchange";
				case ExchangeEdgxExchange:
					return "EdgxExchange";
				case ExchangeChicagoStockExchange:
					return "ChicagoStockExchange";
				case ExchangeNyseEuronext:
				//case ExchangeOptionNyseArca:
					//if (symbolType == SymbolStockOption)
						//return "OptionNyseArca";
					return "NyseEuronext";
				case ExchangeNyseArcaExchange:
					return "NyseArcaExchange";
				case ExchangeNasdaqOmx:
					return "NasdaqOmx";
				case ExchangeCTS:
					return "CTS";
				case ExchangeCTANasdaqOMX:
				//case ExchangeCanadaToronto:
				//case ExchangeOptionNasdaqOmxBx:
					//if (country == CountryCanada)
						//return "CanadaToronto";
					//if (symbolType == SymbolStockOption)
						//return "OptionNasdaqOmxBx";
					return "CTANasdaqOMX";
				case ExchangeOTCBB:
					return "OTCBB";
				case ExchangeNNOTC:
					return "NNOTC";
				case ExchangeChicagoBoardOptionsExchange:
				//case ExchangeOptionC2:
					//if (symbolType == SymbolStockOption)
						//return "OptionC2";
					return "ChicagoBoardOptionsExchange";
				case ExchangeNasdaqOmxPhlx:
					return "NasdaqOmxPhlx";
				case ExchangeBatsYExchange:
					return "BatsYExchange";
				case ExchangeBatsExchange:
					return "BatsExchange";
				case ExchangeCanadaVenture:
					return "CanadaVenture";
				case ExchangeOpra:
					return "Opra";
				case ExchangeComposite:
					return "Composite";
			}
			return "unknown";
		}
	};
}