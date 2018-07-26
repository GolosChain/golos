#pragma once

// macro helpers on GOLOS_CHECK_VALUE and GOLOS_CHECK_PARAM
// to check common cases and autogenerate message text

// Common abbreviations:
//  EQ, NE, LT, GT, LE, GE — like in asm conditions
// (Equal, Not Equal, Less Than, Greather Than, Less or Equal, Greater or Equal)


// GOLOS_CHECK_PARAM helpers
//-------------------------------------------------------------

// check if account parameter valid
#define GOLOS_CHECK_PARAM_ACCOUNT(A)    GOLOS_CHECK_PARAM(A, validate_account_name(A))
// call param internal validate
#define GOLOS_CHECK_PARAM_VALIDATE(P)   GOLOS_CHECK_PARAM(P, P.validate())

// GOLOS_CHECK_VALUE helpers
//-------------------------------------------------------------

// size
#define GOLOS_CHECK_VALUE_MAX_SIZE(F, M) \
    GOLOS_CHECK_VALUE(F.size() <= (M), MUST_BE2(F, "is too long", "<= " FC_STRINGIZE(M)));
#define GOLOS_CHECK_VALUE_NOT_EMPTY(F) \
    GOLOS_CHECK_VALUE(!F.empty(), CANNOT_BE(F, "empty"));
// utf-8
#define GOLOS_CHECK_VALUE_UTF8(F) \
    GOLOS_CHECK_VALUE(fc::is_utf8(F), MUST_BE(F, "valid UTF8 string"));
// json
#define GOLOS_CHECK_VALUE_JSON(F) \
    GOLOS_CHECK_VALUE(fc::json::is_valid(F), MUST_BE(F, "valid JSON"));


// compare field with value
#define GOLOS_CHECK_VALUE_EQ(F, X) GOLOS_CHECK_VALUE_I(F, ==, X)
#define GOLOS_CHECK_VALUE_NE(F, X) GOLOS_CHECK_VALUE_I(F, !=, X)
#define GOLOS_CHECK_VALUE_GT(F, X) GOLOS_CHECK_VALUE_I(F, > , X)
#define GOLOS_CHECK_VALUE_LT(F, X) GOLOS_CHECK_VALUE_I(F, < , X)
#define GOLOS_CHECK_VALUE_GE(F, X) GOLOS_CHECK_VALUE_I(F, >=, X)
#define GOLOS_CHECK_VALUE_LE(F, X) GOLOS_CHECK_VALUE_I(F, <=, X)
#define GOLOS_CHECK_VALUE_LEGE(F, L, H) \
    GOLOS_CHECK_VALUE((L) <= F && F <= (H) , MUST_BE(F, "between " FC_STRINGIZE(L) " and" FC_STRINGIZE(H)))

// check asset type
#define GOLOS_CHECK_ASSET_TYPE(X, NAME) GOLOS_CHECK_ASSET_##NAME(X);
#define GOLOS_CHECK_ASSET_GESTS(X)  GOLOS_CHECK_ASSET_TYPE_I(X, VESTS_SYMBOL, "GESTS")
#define GOLOS_CHECK_ASSET_GOLOS(X)  GOLOS_CHECK_ASSET_TYPE_I(X, STEEM_SYMBOL, "GOLOS")
#define GOLOS_CHECK_ASSET_GBG(X)    GOLOS_CHECK_ASSET_TYPE_I(X, SBD_SYMBOL, "GBG")

#define GOLOS_CHECK_ASSET_GOLOS_OR_GBG(X) \
    GOLOS_CHECK_VALUE(X.symbol == STEEM_SYMBOL || X.symbol == SBD_SYMBOL, MUST_BE(X, "GOLOS or GBG"))

// check asset type and value
#define GOLOS_CHECK_ASSET_GT0(X, NAME) GOLOS_CHECK_ASSET_VAL(X, >, 0, NAME)
#define GOLOS_CHECK_ASSET_GE0(X, NAME) GOLOS_CHECK_ASSET_VAL(X, >=, 0, NAME)
#define GOLOS_CHECK_ASSET_GE(X, NAME, V) GOLOS_CHECK_ASSET_VAL(X, >=, V, NAME)


// internals
//-------------------------------------------------------------

// fields
#define GOLOS_CHECK_VALUE_I(F, OP, X) GOLOS_CHECK_VALUE_II(F, OP, X, F)
#define GOLOS_CHECK_VALUE_II(F, OP, X, N) GOLOS_CHECK_VALUE(F OP (X), MUST_BE(N, "" #OP FC_STRINGIZE(X)))

// asset type
#define GOLOS_CHECK_ASSET_TYPE_I(X, SYMBOL, SNAME)  GOLOS_CHECK_VALUE(X.symbol == SYMBOL, MUST_BE(X, SNAME))

// asset value
#define GOLOS_CHECK_ASSET_VAL(X, OP, V, N) { \
    GOLOS_CHECK_ASSET_TYPE(X, N); \
    GOLOS_CHECK_VALUE_II(X.amount, OP, V, X); \
}

// utils
#define MUST_BE(NAME, REQ) #NAME " must be " REQ
#define MUST_BE2(NAME, DESC, REQ) #NAME DESC ", it must be " REQ
#define CANNOT_BE(NAME, REQ) #NAME " cannot be " REQ
