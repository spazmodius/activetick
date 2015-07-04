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
			StreamUpdateTrade,
			StreamUpdateQuote,
			StreamUpdateRefresh,
			StreamUpdateTopMarketMovers,
			HolidaysResponse,
			ServerTimeUpdate,
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

		static inline bool set(Handle<Object> value, const char* name, const ATTIME& time) {
			return v8set(value, name, convert(time));
		}

		static inline bool set(Handle<Object> value, const char* name, const ATPRICE& price) {
			return v8set(value, name, convert(price));
		}

		static inline bool set(Handle<Object> value, const char* name, ATExchangeType exchange) {
			return v8set(value, name, convert(exchange));
		}

		static inline bool flag(Handle<Object> value, ATTradeConditionType condition) {
			return v8flag(value, convert(condition));
		}

		static inline bool flag(Handle<Object> value, ATQuoteConditionType condition) {
			return v8flag(value, convert(condition));
		}

		static void flags(Handle<Object> value, ATTradeMessageFlags flags) {
			if (flags & TradeMessageFlagRegularMarketLastPrice)
				v8flag(value, "regularMarketLastPrice");
			if (flags & TradeMessageFlagRegularMarketVolume)
				v8flag(value, "regularMarketVolume");
			if (flags & TradeMessageFlagHighPrice)
				v8flag(value, "highPrice");
			if (flags & TradeMessageFlagLowPrice)
				v8flag(value, "lowPrice");
			if (flags & TradeMessageFlagDayHighPrice)
				v8flag(value, "dayHighPrice");
			if (flags & TradeMessageFlagDayLowPrice)
				v8flag(value, "dayLowPrice");
			if (flags & TradeMessageFlagExtendedMarketLastPrice)
				v8flag(value, "extendedMarketLastPrice");
			if (flags & TradeMessageFlagPreMarketVolume)
				v8flag(value, "preMarketVolume");
			if (flags & TradeMessageFlagAfterMarketVolume)
				v8flag(value, "afterMarketVolume");
			if (flags & TradeMessageFlagPreMarketOpenPrice)
				v8flag(value, "preMarketOpenPrice");
			if (flags & TradeMessageFlagOpenPrice)
				v8flag(value, "openPrice");
		}

		static void set(Handle<Object> value, const char* name, ATSymbolType symbolType, ATExchangeType exchangeType, ATCountryType countryType) {
			char type[4];
			type[0] = symbolType;
			type[1] = exchangeType;
			type[2] = countryType;
			type[3] = 0;
			v8set(value, name, type);
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
				case StreamUpdateTrade:
					return "stream-update-trade";
				case StreamUpdateQuote:
					return "stream-update-quote";
				case StreamUpdateRefresh:
					return "stream-update-refresh";
				case StreamUpdateTopMarketMovers:
					return "stream-update-top-market-movers";
				case HolidaysResponse:
					return "holidays-response";
				case ServerTimeUpdate:
					return "server-time-update";
			}
			return "unknown";
		}

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

		static const char* convert(ATQuoteConditionType condition) {
			switch (condition) {
				//case QuoteConditionRegular:
				//	return "Regular";
				case QuoteConditionRegularTwoSidedOpen:
					return "regularTwoSidedOpen";
				case QuoteConditionRegularOneSidedOpen:
					return "regularOneSidedOpen";
				case QuoteConditionSlowAsk:
					return "slowAsk";
				case QuoteConditionSlowBid:
					return "slowBid";
				case QuoteConditionSlowBidAsk:
					return "slowBidAsk";
				case QuoteConditionSlowDueLRPBid:
					return "slowDueLRPBid";
				case QuoteConditionSlowDueLRPAsk:
					return "slowDueLRPAsk";
				case QuoteConditionSlowDueNYSELRP:
					return "slowDueNYSELRP";
				case QuoteConditionSlowDueSetSlowListBidAsk:
					return "slowDueSetSlowListBidAsk";
				case QuoteConditionManualAskAutomaticBid:
					return "manualAskAutomaticBid";
				case QuoteConditionManualBidAutomaticAsk:
					return "manualBidAutomaticAsk";
				case QuoteConditionManualBidAndAsk:
					return "manualBidAndAsk";
				case QuoteConditionOpening:
					return "opening";
				case QuoteConditionClosing:
					return "closing";
				case QuoteConditionClosed:
					return "closed";
				case QuoteConditionResume:
					return "resume";
				case QuoteConditionFastTrading:
					return "fastTrading";
				case QuoteConditionTradingRangeIndication:
					return "tradingRangeIndication";
				case QuoteConditionMarketMakerQuotesClosed:
					return "marketMakerQuotesClosed";
				case QuoteConditionNonFirm:
					return "nonFirm";
				case QuoteConditionNewsDissemination:
					return "newsDissemination";
				case QuoteConditionOrderInflux:
					return "orderInflux";
				case QuoteConditionOrderImbalance:
					return "orderImbalance";
				case QuoteConditionDueToRelatedSecurityNewsDissemination:
					return "dueToRelatedSecurityNewsDissemination";
				case QuoteConditionDueToRelatedSecurityNewsPending:
					return "dueToRelatedSecurityNewsPending";
				case QuoteConditionAdditionalInformation:
					return "additionalInformation";
				case QuoteConditionNewsPending:
					return "newsPending";
				case QuoteConditionAdditionalInformationDueToRelatedSecurity:
					return "additionalInformationDueToRelatedSecurity";
				case QuoteConditionDueToRelatedSecurity:
					return "dueToRelatedSecurity";
				case QuoteConditionInViewOfCommon:
					return "inViewOfCommon";
				case QuoteConditionEquipmentChangeover:
					return "equipmentChangeover";
				case QuoteConditionNoOpenNoResume:
					return "noOpenNoResume";
				case QuoteConditionSubPennyTrading:
					return "subPennyTrading";
				case QuoteConditionAutomatedBidNoOfferNoBid:
					return "automatedBidNoOfferNoBid";
			}
			return NULL;
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
			set(value, "serverTime", response.serverTime);
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
		ATQUOTESTREAM_DATA_ITEM item;

		QuoteStreamResponseMessage(uint64_t session, uint64_t request, ATQUOTESTREAM_RESPONSE& response, ATQUOTESTREAM_DATA_ITEM& item) :
			Message(QuoteStreamResponse, session, request),
			response(response),
			item(item)
		{}

		void populate(Handle<Object> value) {
			v8set(value, "streamResponse", convert(response.responseType));
			v8set(value, "symbol", item.symbol.symbol);
			v8set(value, "symbolStatus", convert(item.symbolStatus));
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
			set(value, "time", trade.lastDateTime);
			v8set(value, "symbol", trade.symbol.symbol);
			set(value, "price", trade.lastPrice);
			v8set(value, "size", trade.lastSize);
			set(value, "exchange", trade.lastExchange);
			for (int i = 0; i < ATTradeConditionsCount; ++i)
				flag(value, trade.condition[i]);
			flags(value, trade.flags);
		}
	};

	struct StreamUpdateQuoteMessage : Message {
		ATQUOTESTREAM_QUOTE_UPDATE quote;

		StreamUpdateQuoteMessage(ATQUOTESTREAM_QUOTE_UPDATE& quote) :
			Message(StreamUpdateQuote),
			quote(quote)
		{}

		void populate(Handle<Object> value) {
			set(value, "time", quote.quoteDateTime);
			v8set(value, "symbol", quote.symbol.symbol);

			set(value, "bidPrice", quote.bidPrice);
			v8set(value, "bidSize", quote.bidSize);
			set(value, "bidExchange", quote.bidExchange);

			set(value, "askPrice", quote.askPrice);
			v8set(value, "askSize", quote.askSize);
			set(value, "askExchange", quote.askExchange);

			flag(value, quote.condition);
		}
	};

	struct StreamUpdateRefreshMessage : Message {
		ATQUOTESTREAM_REFRESH_UPDATE refresh;

		StreamUpdateRefreshMessage(ATQUOTESTREAM_REFRESH_UPDATE& refresh) :
			Message(StreamUpdateRefresh),
			refresh(refresh)
		{}

		void populate(Handle<Object> value) {
			v8set(value, "symbol", refresh.symbol.symbol);

			v8set(value, "volume", (double)refresh.volume);
			set(value, "openPrice", refresh.openPrice);
			set(value, "highPrice", refresh.highPrice);
			set(value, "lowPrice", refresh.lowPrice);
			set(value, "closePrice", refresh.closePrice);
			set(value, "prevClosePrice", refresh.prevClosePrice);
			set(value, "afterMarketClosePrice", refresh.afterMarketClosePrice);

			set(value, "lastPrice", refresh.lastPrice);
			v8set(value, "lastSize", refresh.lastSize);
			set(value, "lastExchange", refresh.lastExchange);
			for (int i = 0; i < ATTradeConditionsCount; ++i)
				flag(value, refresh.lastCondition[i]);

			set(value, "bidPrice", refresh.bidPrice);
			v8set(value, "bidSize", refresh.bidSize);
			set(value, "bidExchange", refresh.bidExchange);

			set(value, "askPrice", refresh.askPrice);
			v8set(value, "askSize", refresh.askSize);
			set(value, "askExchange", refresh.askExchange);

			flag(value, refresh.quoteCondition);
		}
	};

	struct HolidaysResponseMessage : Message {
		ATMARKET_HOLIDAYSLIST_ITEM item;

		HolidaysResponseMessage(uint64_t session, uint64_t request, ATMARKET_HOLIDAYSLIST_ITEM& item) :
			Message(HolidaysResponse, session, request),
			item(item)
		{}

		void populate(Handle<Object> value) {
			set(value, "exchanges", item.symbolType, item.exchangeType, item.countryType);
			set(value, "begins", item.beginDateTime);
			set(value, "ends", item.endDateTime);
		}
	};

	struct ServerTimeUpdateMessage : Message {
		ATTIME time;

		ServerTimeUpdateMessage(ATTIME& time) :
			Message(ServerTimeUpdate),
			time(time)
		{}

		void populate(Handle<Object> value) {
			set(value, "time", time);
		}
	};
}