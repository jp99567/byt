#pragma once

#include<map>
#include <utility>
#include <strings.h>

namespace grammar {

enum class kw {
  DATE,
  INTERVAL,
  CONSTANT,
  THEN,
  END_FOR,
  IN,
  VAR_OUTPUT,
  L,
  STRUCT,
  AT,
  NOT,
  VAR_IN_OUT,
  STRING,
  PROGRAM,
  DT,
  END_PROGRAM,
  TP,
  R,
  DO,
  FALSE,
  TRUE,
  ACTION,
  REAL,
  ABS,
  DATE_AND_TIME,
  USINT,
  CLK,
  TIME,
  FUNCTION,
  ON,
  BYTE,
  TON,
  INT,
  ELSE,
  MAX,
  S,
  UDINT,
  VAR_GLOBAL,
  SINT,
  END_VAR,
  END_STRUCT,
  TO,
  VAR_INPUT,
  MIN,
  N,
  R_EDGE,
  REPLACE,
  LEN,
  XOR,
  OR,
  F_TRIG,
  CASE,
  TYPE,
  FOR,
  FUNCTION_BLOCK,
  R_TRIG,
  END_TYPE,
  BOOL,
  MOD,
  TOD,
  IF,
  ET,
  ELSIF,
  OF,
  D,
  UINT,
  ARRAY,
  PT,
  LEFT,
  P,
  VAR,
  LOG,
  F_EDGE,
  END_IF,
  WORD,
  Q,
  TOF,
  SHL,
  DELETE,
  LD,
  END_CASE,
  END_FUNCTION_BLOCK,
  EXIT,
  END_FUNCTION,
  SHR,
  RIGHT,
  MID,
  VAR_TEMP,
 _Last };

template<typename _Tp>
  struct ICCMP : public std::binary_function<_Tp, _Tp, bool>
  {
    _GLIBCXX14_CONSTEXPR
    bool
    operator()(const _Tp& lhs, const _Tp& rhs) const
    { return 0 > strcasecmp(lhs.c_str(), rhs.c_str());}
  };

using KwMap = std::map<std::string,kw,ICCMP<std::string>>;

KwMap keywordsMap()
{
	KwMap rv;
    rv.emplace(std::make_pair( "DATE", kw::DATE ));
    rv.emplace(std::make_pair( "INTERVAL", kw::INTERVAL ));
    rv.emplace(std::make_pair( "CONSTANT", kw::CONSTANT ));
    rv.emplace(std::make_pair( "THEN", kw::THEN ));
    rv.emplace(std::make_pair( "END_FOR", kw::END_FOR ));
    rv.emplace(std::make_pair( "IN", kw::IN ));
    rv.emplace(std::make_pair( "VAR_OUTPUT", kw::VAR_OUTPUT ));
    rv.emplace(std::make_pair( "L", kw::L ));
    rv.emplace(std::make_pair( "STRUCT", kw::STRUCT ));
    rv.emplace(std::make_pair( "AT", kw::AT ));
    rv.emplace(std::make_pair( "NOT", kw::NOT ));
    rv.emplace(std::make_pair( "VAR_IN_OUT", kw::VAR_IN_OUT ));
    rv.emplace(std::make_pair( "STRING", kw::STRING ));
    rv.emplace(std::make_pair( "PROGRAM", kw::PROGRAM ));
    rv.emplace(std::make_pair( "DT", kw::DT ));
    rv.emplace(std::make_pair( "END_PROGRAM", kw::END_PROGRAM ));
    rv.emplace(std::make_pair( "TP", kw::TP ));
    rv.emplace(std::make_pair( "R", kw::R ));
    rv.emplace(std::make_pair( "DO", kw::DO ));
    rv.emplace(std::make_pair( "FALSE", kw::FALSE ));
    rv.emplace(std::make_pair( "TRUE", kw::TRUE ));
    rv.emplace(std::make_pair( "ACTION", kw::ACTION ));
    rv.emplace(std::make_pair( "REAL", kw::REAL ));
    rv.emplace(std::make_pair( "ABS", kw::ABS ));
    rv.emplace(std::make_pair( "DATE_AND_TIME", kw::DATE_AND_TIME ));
    rv.emplace(std::make_pair( "USINT", kw::USINT ));
    rv.emplace(std::make_pair( "CLK", kw::CLK ));
    rv.emplace(std::make_pair( "TIME", kw::TIME ));
    rv.emplace(std::make_pair( "FUNCTION", kw::FUNCTION ));
    rv.emplace(std::make_pair( "ON", kw::ON ));
    rv.emplace(std::make_pair( "BYTE", kw::BYTE ));
    rv.emplace(std::make_pair( "TON", kw::TON ));
    rv.emplace(std::make_pair( "INT", kw::INT ));
    rv.emplace(std::make_pair( "ELSE", kw::ELSE ));
    rv.emplace(std::make_pair( "MAX", kw::MAX ));
    rv.emplace(std::make_pair( "S", kw::S ));
    rv.emplace(std::make_pair( "UDINT", kw::UDINT ));
    rv.emplace(std::make_pair( "VAR_GLOBAL", kw::VAR_GLOBAL ));
    rv.emplace(std::make_pair( "SINT", kw::SINT ));
    rv.emplace(std::make_pair( "END_VAR", kw::END_VAR ));
    rv.emplace(std::make_pair( "END_STRUCT", kw::END_STRUCT ));
    rv.emplace(std::make_pair( "TO", kw::TO ));
    rv.emplace(std::make_pair( "VAR_INPUT", kw::VAR_INPUT ));
    rv.emplace(std::make_pair( "MIN", kw::MIN ));
    rv.emplace(std::make_pair( "N", kw::N ));
    rv.emplace(std::make_pair( "R_EDGE", kw::R_EDGE ));
    rv.emplace(std::make_pair( "REPLACE", kw::REPLACE ));
    rv.emplace(std::make_pair( "LEN", kw::LEN ));
    rv.emplace(std::make_pair( "XOR", kw::XOR ));
    rv.emplace(std::make_pair( "OR", kw::OR ));
    rv.emplace(std::make_pair( "F_TRIG", kw::F_TRIG ));
    rv.emplace(std::make_pair( "CASE", kw::CASE ));
    rv.emplace(std::make_pair( "TYPE", kw::TYPE ));
    rv.emplace(std::make_pair( "FOR", kw::FOR ));
    rv.emplace(std::make_pair( "FUNCTION_BLOCK", kw::FUNCTION_BLOCK ));
    rv.emplace(std::make_pair( "R_TRIG", kw::R_TRIG ));
    rv.emplace(std::make_pair( "END_TYPE", kw::END_TYPE ));
    rv.emplace(std::make_pair( "BOOL", kw::BOOL ));
    rv.emplace(std::make_pair( "MOD", kw::MOD ));
    rv.emplace(std::make_pair( "TOD", kw::TOD ));
    rv.emplace(std::make_pair( "IF", kw::IF ));
    rv.emplace(std::make_pair( "ET", kw::ET ));
    rv.emplace(std::make_pair( "ELSIF", kw::ELSIF ));
    rv.emplace(std::make_pair( "OF", kw::OF ));
    rv.emplace(std::make_pair( "D", kw::D ));
    rv.emplace(std::make_pair( "UINT", kw::UINT ));
    rv.emplace(std::make_pair( "ARRAY", kw::ARRAY ));
    rv.emplace(std::make_pair( "PT", kw::PT ));
    rv.emplace(std::make_pair( "LEFT", kw::LEFT ));
    rv.emplace(std::make_pair( "P", kw::P ));
    rv.emplace(std::make_pair( "VAR", kw::VAR ));
    rv.emplace(std::make_pair( "LOG", kw::LOG ));
    rv.emplace(std::make_pair( "F_EDGE", kw::F_EDGE ));
    rv.emplace(std::make_pair( "END_IF", kw::END_IF ));
    rv.emplace(std::make_pair( "WORD", kw::WORD ));
    rv.emplace(std::make_pair( "Q", kw::Q ));
    rv.emplace(std::make_pair( "TOF", kw::TOF ));
    rv.emplace(std::make_pair( "SHL", kw::SHL ));
    rv.emplace(std::make_pair( "DELETE", kw::DELETE ));
    rv.emplace(std::make_pair( "LD", kw::LD ));
    rv.emplace(std::make_pair( "END_CASE", kw::END_CASE ));
    rv.emplace(std::make_pair( "END_FUNCTION_BLOCK", kw::END_FUNCTION_BLOCK ));
    rv.emplace(std::make_pair( "EXIT", kw::EXIT ));
    rv.emplace(std::make_pair( "END_FUNCTION", kw::END_FUNCTION ));
    rv.emplace(std::make_pair( "SHR", kw::SHR ));
    rv.emplace(std::make_pair( "RIGHT", kw::RIGHT ));
    rv.emplace(std::make_pair( "MID", kw::MID ));
    rv.emplace(std::make_pair( "VAR_TEMP", kw::END_VAR ));
	return rv;
}

using BlockMap = std::map<kw,kw>;

BlockMap blockMap()
{
	BlockMap rv;

	rv.emplace(std::make_pair( kw::VAR, kw::END_VAR ));
	rv.emplace(std::make_pair( kw::VAR_GLOBAL, kw::END_VAR ));
	rv.emplace(std::make_pair( kw::VAR_INPUT, kw::END_VAR ));
	rv.emplace(std::make_pair( kw::VAR_OUTPUT, kw::END_VAR ));
	rv.emplace(std::make_pair( kw::VAR_IN_OUT, kw::END_VAR ));
	rv.emplace(std::make_pair( kw::VAR_TEMP, kw::END_VAR ));
	rv.emplace(std::make_pair( kw::TYPE, kw::END_TYPE ));
	rv.emplace(std::make_pair( kw::STRUCT, kw::END_STRUCT ));
	rv.emplace(std::make_pair( kw::FUNCTION_BLOCK, kw::END_FUNCTION_BLOCK ));
	rv.emplace(std::make_pair( kw::FUNCTION, kw::END_FUNCTION ));
	rv.emplace(std::make_pair( kw::PROGRAM, kw::END_PROGRAM ));
	return rv;
}

constexpr auto varDeclBlockList =
{
	kw::VAR,
	kw::VAR_GLOBAL,
	kw::VAR_INPUT,
	kw::VAR_OUTPUT,
	kw::VAR_IN_OUT,
	kw::VAR_TEMP,
	kw::TYPE,
	kw::STRUCT
};

}
