#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <fstream>
#include <sstream>
#include <QtCore/QRegularExpression>
#include <spdlog/spdlog.h>
#include "grammar.h"

std::shared_ptr<spdlog::logger> logr;
#define LogDBG(...) logr->debug(__VA_ARGS__)
#define LogERR(...) logr->error(__VA_ARGS__)

class Grammar
{
	Grammar()
	:kwmap(grammar::keywordsMap())
	,blockmap(grammar::blockMap())
	{
	}

public:
	static Grammar& inst()
	{
		static Grammar _;
		return _;
	}
	const grammar::KwMap kwmap;
	const grammar::BlockMap blockmap;

	bool isBlock(grammar::kw kw) const
	{
		auto res = blockmap.find(kw);
		return res != std::cend(blockmap);
	}

	grammar::kw blockEnd(grammar::kw block) const
	{
		return blockmap.at(block);
	}

	static bool isVarDeclBlock(grammar::kw kw)
	{
		auto res = std::find(std::cbegin(grammar::varDeclBlockList), std::cend(grammar::varDeclBlockList), kw);
		return res != std::cend(grammar::varDeclBlockList);
	}
};

struct Token
{
	enum class Type
	{
		ws, comment_block, comment_line, kw, oper, ident, liter
	};

	Type type;
	std::string s;
	std::size_t srcline;
	grammar::kw kw() const
	{
		if(type != Type::kw)
			return grammar::kw::_Last;

		return Grammar::inst().kwmap.at(s);
	}

	void transformLit()
	{
		if(s[0] == '\''){
			s[0] = '"';
			s.back() = '"';
			auto pos = s.find("$n");
			if(pos != std::string::npos)
				s.replace(pos, 2, "\\n");
		}
	}
	void gencpp()
	{
		switch(type)
		{
		case Type::ws:
			break;
		case Type::comment_line:
			s = std::string("//").append(s).append("\n");
			break;
		case Type::comment_block:
			s = std::string("/*").append(s).append("*/");
			break;
		case Type::kw:
		  {
			auto k = kw();
			switch(k)
			{
			case grammar::kw::EXIT:
				s = "break";
				break;
			default:
				break;
			}
		  }
		  break;
		case Type::oper:
		{
			if(s == ":=")
				s = "=";
			else if(s == "=")
				s = "==";
			else if(s == "<>")
				s = "!=";
		}
			break;
		case Type::ident:
			break;
		case Type::liter:
			transformLit();
			break;
		}
	}

	std::string dump() const
	{
		std::ostringstream ss;

		ss << "line:" << srcline << " (" << (int)type << ")";
		switch(type)
		{
		case Type::ws:
			ss << "blank";
			break;
		case Type::comment_line:
		case Type::kw:
		case Type::oper:
		case Type::ident:
		case Type::liter:
			ss << s;
			break;
		case Type::comment_block:
			ss << "comment block";
			break;
		default:
			LogERR("token dump fuckup");
			break;
		}

		return ss.str();
	}

	void logDump() const
	{
		switch(type)
		{
		case Type::ws:
			LogDBG("{}\tblank: '{}'", srcline, s);
			break;
		case Type::comment_line:
			LogDBG("{}\tcomment line: --{}", srcline, s);
			break;
		case Type::comment_block:
			LogDBG("{}\tcomment block: /*{}*/", srcline, s);
			break;
		case Type::kw:
			LogDBG("{}\tkw: ={}='", srcline, s);
			break;
		case Type::oper:
			LogDBG("{}\toper: x {} x", srcline, s);
			break;
		case Type::ident:
			LogDBG("{}\tident: = {} =", srcline, s);
			break;
		case Type::liter:
			LogDBG("{}\tliter: = {} =", srcline, s);
			break;
		default:
			LogERR("token log dump fuckup");
			break;
		}
	}
};

class Parser
{
	std::string left;
	std::string line;
	std::size_t lineNr = 0;

	const QRegularExpression wordRe;
	const QRegularExpression wspaceRe;
	const QRegularExpression literRe;

	std::istream& input;
public:

	explicit Parser(std::istream& input)
	:wordRe("^(\\w[\\w\\d#]*)(.*)")
	,wspaceRe("^(\\s+)(.*)")
	,literRe("^([^\\w\\s]+)(.*)")
	,input(input)
	{}

	struct Chunk
	{
		enum class Type { EF, BLANK, WORD , NonW };
		Type type = Type::EF;
		std::string s;
	};

	Chunk chunk()
	{
		if(left.empty())
		{
			if( not std::getline(input, line)){
				return Chunk();
			}
			++lineNr;
			if(line.empty()){
				return Chunk({Chunk::Type::BLANK, "\n"});
			}
			left = line;
		}

		auto match = wordRe.match(QString::fromStdString(left));
		if(match.hasMatch()){
			left = match.captured(2).toStdString();
			return Chunk({Chunk::Type::WORD, match.captured(1).toStdString()});
		}

		match = wspaceRe.match(QString::fromStdString(left));
		if(match.hasMatch()){
			left = match.captured(2).toStdString();
			return Chunk({Chunk::Type::BLANK, match.captured(1).toStdString()});
		}

		match = literRe.match(QString::fromStdString(left));
		if(match.hasMatch()){
			left = match.captured(2).toStdString();
			return Chunk({Chunk::Type::NonW, match.captured(1).toStdString()});
		}

		throw std::invalid_argument("document fuckup");

		return Chunk();
	}

	std::string getRestOfLine()
	{
		return std::move(left);
	}

	std::string getfLine() const
	{
		return line;
	}

	std::size_t getfLineNr() const
	{
		return lineNr;
	}
};

std::vector<Token> parse(Parser& p)
{
	Parser::Chunk chunk;
	std::vector<Token> tv;
	const QRegularExpression reIdentif("^[A-Za-z][\\w\\d]*$");

	while(1){
		chunk = p.chunk();
		if(chunk.type == Parser::Chunk::Type::EF)
			break;

		if(chunk.type == Parser::Chunk::Type::BLANK){
			tv.emplace_back(Token({Token::Type::ws, chunk.s, p.getfLineNr()}));
		}
		else if(chunk.type == Parser::Chunk::Type::WORD){
			auto res = Grammar::inst().kwmap.find(chunk.s);
			if(res != std::cend(Grammar::inst().kwmap))
			{
				tv.emplace_back(Token({Token::Type::kw, std::move(chunk.s), p.getfLineNr()}));
			}
			else{
				auto qstr = QString::fromStdString(chunk.s);
				auto identif = reIdentif.match(qstr);
				if(identif.hasMatch()){
					tv.emplace_back(Token({Token::Type::ident, qstr.toLower().toStdString(), p.getfLineNr()}));
				}
				else{
					tv.emplace_back(Token({Token::Type::liter, std::move(chunk.s), p.getfLineNr()}));
				}
			}
		}
		else if(chunk.type == Parser::Chunk::Type::NonW){
			auto& s = chunk.s;

			while(not s.empty())
			{
			  if(0 == s.compare(0,2,"//"))
			  {
				tv.emplace_back(Token({Token::Type::comment_line, s.substr(2)+p.getRestOfLine(), p.getfLineNr()}));
				s = std::string();
			  }
			  else if(0 == s.compare(0,2,"(*")){
				std::string vc;
				s.erase(0,2);

				auto isEOComment = [&chunk,&vc]{
					if(chunk.type == Parser::Chunk::Type::EF)
					{
						throw std::invalid_argument("fuckup end of block comment missing");
					}
					if(chunk.type != Parser::Chunk::Type::NonW)
					{
						vc.append(std::move(chunk.s));
						return false;
					}

					auto res = chunk.s.find("*)");
					if( res != std::string::npos)
					{
						vc.append(chunk.s.substr(0,res));
						chunk.s.erase(0,res+2);
						return true;
					}
					return false;
				};

				while(not isEOComment()){
					chunk = p.chunk();
				}

				tv.emplace_back(Token({Token::Type::comment_block, std::move(vc), p.getfLineNr()}));
			  }
			  else if(0 == s.compare(0,1,"'")){
				  std::string strlit("'");
				  s.erase(0,1);

				  auto isEOStrLit = [&chunk,&strlit]{
					  if(chunk.type == Parser::Chunk::Type::EF)
					  {
				  		throw std::invalid_argument("fuckup end of string literal missing");
					  }
					  if(chunk.type != Parser::Chunk::Type::NonW)
					  {
						  strlit.append(std::move(chunk.s));
				  	  	  return false;
				  	  }

				  	  auto res = chunk.s.find("'");
				  	  if( res != std::string::npos)
				  	  {
				  		strlit.append(chunk.s.substr(0,res+1));
				  		chunk.s.erase(0,res+1);
				  		return true;
				  	  }
				  	  return false;
			  	  };

				  while(not isEOStrLit()){
					  chunk = p.chunk();
				  }

			  tv.emplace_back(Token({Token::Type::liter, std::move(strlit), p.getfLineNr()}));

			  }
			  else{
				  auto res = std::find_if(std::cbegin(grammar::nwopers),std::cend(grammar::nwopers),[&chunk](auto& kw){
					  return 0 == chunk.s.compare(0, strlen(kw), kw);
				  });

				  if(res == std::cend(grammar::nwopers))
				  {
					  throw std::invalid_argument("fuckup invalid operator");
				  }
				  else{
					  tv.emplace_back(Token({Token::Type::oper, *res, p.getfLineNr()}));
					  chunk.s.erase(0,strlen(*res));
				  }
			  }
			}
		}
	};

	return tv;
}

struct Tokens
{
	std::size_t cur = 0;
	std::vector<Token> tok;
};

struct Entit;
struct NonCode;
using EntitPtr = std::unique_ptr<Entit>;

struct Entit
{
	enum class Type { UNIT, BLOCK, NONCODE, STATEMENT, VARDEC, COMMAND };
	Entit(Type type) :type(type){}
	std::vector<EntitPtr> subent;
	std::vector<Token> noncode;
	Type type;
	virtual void parseTokens(Tokens& t) = 0;
	virtual std::vector<std::string> gencpp() = 0;
	virtual ~Entit(){}

	virtual void dump() = 0;
};

struct NonCode : Entit
{
	NonCode():Entit(Type::NONCODE){}
	std::vector<Token> tv;
	void parseTokens(Tokens& t) override {};
	void dump() override
	{
		for(auto& t : tv)
			t.dump();
	}

	std::vector<std::string> gencpp() override
	{
		std::vector<std::string> rv;
		for(auto& t : tv)
		{
			t.gencpp();
			rv.emplace_back(std::move(t.s));
		}
		return rv;
	}
};

static bool isNonCodeType(const Token::Type type)
{
	return type == Token::Type::comment_block ||
		   type == Token::Type::comment_line||
		   type == Token::Type::ws;
}

struct Unit : Entit
{
	Unit():Entit(Type::UNIT){}
	Unit(Type type):Entit(type){}
	bool checkNonCode(const Token& tok);
	bool nonCodeSimpleCheck(const Token& tok)
	{
		if( isNonCodeType(tok.type) )
		{
			addCommentOnly(tok);
			return true;
		}
		return false;
	}

	void addCommentOnly(const Token& tok)
	{
		if(tok.type == Token::Type::comment_block
		 ||tok.type == Token::Type::comment_line )
			noncode.emplace_back(tok);
	}

	void parseTokens(Tokens& t) override;

	void dump() override
	{
		for(auto& e : subent)
			e->dump();
	}

	void appendNonCode()
	{
		if(not noncode.empty())
		{
			auto pp = new NonCode;
			pp->tv = std::move(noncode);
			subent.emplace_back(pp);
		}
	}

	std::vector<std::string> gencpp() override
	{
		appendNonCode();
		std::vector<std::string> rv;
		for(auto& e : subent)
		{
			auto l = e->gencpp();
			std::move(std::begin(l), std::end(l), std::back_inserter(rv));
		}
		subent.clear();
		return rv;
	}
};

bool Unit::checkNonCode(const Token& tok)
{
	if(isNonCodeType(tok.type))
	{
		addCommentOnly(tok);
		return true;
	}

	appendNonCode();

	return false;
}

struct Command : Unit
{
	std::vector<Token> tv;

	explicit Command() : Unit(Type::COMMAND){}

	void parseTokens(Tokens& t) override
	{
		while(t.cur < t.tok.size())
		{
			auto& tok = t.tok[t.cur++];
			if(tok.type == Token::Type::oper &&
			   tok.s == ";")
			{
					return;
			}
			else if(not nonCodeSimpleCheck(tok))
			{
				tv.emplace_back(tok);
			}
		}

		throw std::logic_error("Command reached the end");
	}

	std::vector<std::string> gencpp() override
	{
			std::vector<std::string> rv;
			auto nonBlank = std::find_if(std::crbegin(tv), std::crend(tv), [](auto t){
				return not isNonCodeType(t.type);
			});

			std::for_each(std::cbegin(tv), std::cend(tv), [nonBlank,&rv](auto t){
				auto& ct = *nonBlank;
				if( &t == &ct )
				{
					rv.emplace_back(std::string(std::move(t.s).append(";")));
				}
				else
				{
					rv.emplace_back(std::string(std::move(t.s)));
				}
			});
			rv.back().append("\n");
			return rv;
	}
};

using CommandPtr = std::unique_ptr<Command>;

struct Statement : Unit
{
	using EndCond = std::function<bool(Tokens& t)>;

	EndCond isFinished;

	Statement(EndCond f)
	:Unit(Type::STATEMENT)
	,isFinished(f){}

	void parseTokens(Tokens& t) override;

	void dump() override
	{
	}
};

using StatementPtr = std::unique_ptr<Statement>;

struct ForBlock : Unit
{
	std::vector<Token> initcond;
	StatementPtr statement;
	bool isInitCond = true;

	ForBlock() : Unit(Type::COMMAND){}

	void parseTokens(Tokens& t) override;
	std::vector<std::string> gencpp() override
	{
			std::vector<std::string> rv;
			//ToDo
			return rv;
	}
};

struct CaseBlock : Unit
{
	std::vector<Token> swexpr;
	std::vector<std::vector<Token>> casecond;
	std::vector<CommandPtr> casecommand;
	CommandPtr elsecommand;
	bool isAfterEndCase = false;

	CaseBlock() : Unit(Type::COMMAND)
	{
	}

	void parseTokens(Tokens& t) override;
	std::vector<std::string> gencpp() override
		{
				std::vector<std::string> rv;
				//ToDo
				return rv;
		}
};

struct IfBlock : Unit
{
	std::vector<std::vector<Token>> cond;
	std::vector<StatementPtr> statement;
	StatementPtr elsestatement;
	bool isAfterEndIf = false;

	IfBlock() : Unit(Type::COMMAND)
	{
		cond.emplace_back(std::vector<Token>());
	}

	void parseTokens(Tokens& t) override;
	std::vector<std::string> gencpp() override
		{
				std::vector<std::string> rv;
				//ToDo
				return rv;
		}
};

struct Block : Unit
{
	grammar::kw block;
	std::vector<Token> ident;
	const std::size_t identSize;

	constexpr std::size_t calcIdentSize(grammar::kw block)
	{
		if(block == grammar::kw::PROGRAM ||
		   block == grammar::kw::FUNCTION_BLOCK)
			return 1;
		else if(block == grammar::kw::FUNCTION)
			return 3;
		return 0;
	}

	Block(grammar::kw kw)
	:Unit(Type::BLOCK)
	,block(kw)
	,identSize(calcIdentSize(block))
	{}

	void parseTokens(Tokens& t) override
	{
		while(t.cur < t.tok.size())
		{
			auto& tok = t.tok[t.cur++];
			if(! checkNonCode(tok) ){
				LogDBG("Block token: {} {}", tok.s, tok.srcline);
				if(tok.type == Token::Type::kw)
				{
					if(Grammar::inst().blockEnd(block) == tok.kw() )
					{
						return;
					}
					else if(Grammar::inst().isBlock(tok.kw()) )
					{
						auto block = new Block(tok.kw());
						block->parseTokens(t);
						subent.emplace_back(block);
						continue;
					}
				}

				if(ident.size() < identSize)
				{
					ident.emplace_back(tok);
				}
				else if(Grammar::isVarDeclBlock(block))
				{
					//ToDo
				}
				else
				{
					--t.cur;
					auto p = new Statement([end_kw = Grammar::inst().blockEnd(block)](Tokens& t){
						auto& tok = t.tok[t.cur];
						LogDBG("check end {} {} {}", tok.s, (int)tok.kw(), (int)end_kw);
						return tok.type == Token::Type::kw && tok.kw() == end_kw;
					});

					p->parseTokens(t);
					subent.emplace_back(p);
				}
			}
		}
		throw std::logic_error("Block reached the end");
	}

	void dump() override
	{
		auto res = std::find_if(std::cbegin(Grammar::inst().kwmap), std::cend(Grammar::inst().kwmap),[this](auto& m){
			return m.second == block;
		});

		std::string bname = "UNKNOWN";

		if(res != std::cend(Grammar::inst().kwmap))
			bname = res->first;

		LogDBG("block:{} subent count:{}", bname, subent.size());
	}
	/*
	std::vector<std::string> gencpp() override
		{
				std::vector<std::string> rv;
				//ToDo
				return rv;
		}
	*/
};

void Unit::parseTokens(Tokens& t)
{
	while(t.cur < t.tok.size())
	{
		auto& tok = t.tok[t.cur++];

		if(! checkNonCode(tok)) {
			if(tok.type == Token::Type::kw &&
	           Grammar::inst().isBlock(tok.kw()) ) {

				auto block = new Block(tok.kw());
				block->parseTokens(t);
				subent.emplace_back(block);
			}
			else {
				LogERR("unexpected token {} {}", tok.srcline, tok.s);
				throw std::invalid_argument("fuckup in unit");
			}
		}
	}
}

void Statement::parseTokens(Tokens& t)
{
	while(t.cur < t.tok.size())
	{
		if(isFinished(t))
			return;

		auto& tok = t.tok[t.cur++];
		if(! checkNonCode(tok) ){
			LogDBG("check6 {} {}", tok.srcline, tok.s);
			if(tok.type == Token::Type::kw)
			{
				if(tok.kw() == grammar::kw::FOR )
				{
					auto p = new ForBlock;
					p->parseTokens(t);
					subent.emplace_back(p);
				}
				else if(tok.kw() == grammar::kw::IF )
				{
					auto p = new IfBlock;
					p->parseTokens(t);
					subent.emplace_back(p);
				}
				else if(tok.kw() == grammar::kw::CASE )
				{
					auto p = new CaseBlock;
					p->parseTokens(t);
					subent.emplace_back(p);
				}
				else if(tok.kw() == grammar::kw::EXIT ){
					goto process_commands;
				}
				else
				{
					LogERR("invalid statement keyword: {} {}", tok.s, tok.srcline );
					throw std::logic_error("invalid statement");
				}
			}
			else
			{
process_commands:
				--t.cur;
				auto p = new Command();
				p->parseTokens(t);
				subent.emplace_back(p);
			}
		}
	}

	throw std::logic_error("Statement reached the end");
}

void ForBlock::parseTokens(Tokens& t)
{
	while(t.cur < t.tok.size())
	{
		auto& tok = t.tok[t.cur++];
		if(isInitCond)
		{
			if(tok.type == Token::Type::kw &&
			   tok.kw() == grammar::kw::DO)
			{
				statement.reset(new Statement([](Tokens& t){
					auto& tok = t.tok[t.cur];
					return tok.type == Token::Type::kw && tok.kw() == grammar::kw::END_FOR;
				}));
				statement->parseTokens(t);
				isInitCond = false;
				++t.cur;
			}
			else
			{
				initcond.emplace_back(tok);
			}
		}
		else
		{
			if(tok.type == Token::Type::oper &&
			   tok.s == ";")
			{
				return;
			}
			else if(!checkNonCode(tok))
			{
				throw std::invalid_argument("fuckup unexpected after for_end");
			}
		}
	}

	throw std::invalid_argument("ForBlock reached the end");
}

void CaseBlock::parseTokens(Tokens& t)
{
	while(t.cur < t.tok.size())
	{
		auto& tok = t.tok[t.cur++];
		if(tok.type == Token::Type::kw &&
		   tok.kw() == grammar::kw::OF)
		{
			break;
		}
		else if(not nonCodeSimpleCheck(tok))
		{
			swexpr.emplace_back(tok);
		}
	}

	LogDBG("case check1:{}", t.tok[t.cur].dump());
	std::vector<Token> tmpcond;
	while( t.cur < t.tok.size() )
	{
		auto& tok = t.tok[t.cur++];
		if( not isAfterEndCase)
		{
			if(tok.type == Token::Type::oper &&
			   tok.s == ":")
			{
				if(tmpcond.size() == 0)
				{
					LogERR("case condition err0", tok.dump() );
					throw std::logic_error("invalid statement");
				}
				if(tmpcond[0].type != Token::Type::liter)
				{
					LogERR("case condition err1", tok.dump() );
					throw std::logic_error("invalid statement");
				}
				if(tmpcond.size() > 1 &&
				   tmpcond.size() %2 !=	1)
				{
					LogERR("case condition err3", tok.dump() );
					throw std::logic_error("invalid statement");
				}
				for(std::size_t i=0; i< tmpcond.size(); ++i)
				{
					casecond.emplace_back(std::vector<Token>());
					if( (i+1) % 2 )
					{
						if(tmpcond[0].type == Token::Type::liter)
						{
							casecond.back().emplace_back(std::move(tmpcond[i]));
						}
						else
						{
							LogERR("case condition err3", tok.dump() );
							throw std::logic_error("invalid statement");
						}
						tmpcond.clear();
					}
					else
					{
						if(tmpcond[i].type != Token::Type::oper
					     ||tmpcond[i].s != ",")
						{
							LogERR("case condition err4", tok.dump() );
							throw std::logic_error("invalid statement");
						}
					}
				}

				auto p = new Command;
				p->parseTokens(t);
				casecommand.emplace_back(p);
			}
			else if(tok.type == Token::Type::kw &&
					tok.kw() == grammar::kw::ELSE)
			{
				auto p = new Command;
				p->parseTokens(t);
				elsecommand.reset(p);
			}
			else if(tok.type == Token::Type::kw &&
					tok.kw() == grammar::kw::END_CASE)
			{
				if(casecond.size() != casecommand.size())
				{
					LogERR("case mismatch {}<>{} {}", casecond.size(), casecommand.size(), tok.dump() );
					throw std::logic_error("invalid statement");
				}
				isAfterEndCase = true;
			}
			else if( not nonCodeSimpleCheck(tok) )
			{
				if(tok.type != Token::Type::liter &&
				   (tok.type != Token::Type::oper || tok.s != ","))
				{
					LogERR("invalid case expr: {}", tok.dump() );
					throw std::logic_error("invalid statement");
				}

				tmpcond.emplace_back(tok);
			}
		}
		else
		{
			if(tok.type == Token::Type::oper &&
			   tok.s == ";")
			{
				return;
			}
			else if(!checkNonCode(tok))
			{
				LogERR("fuckup end of CaseBlock at {} {}", tok.s, tok.srcline);
				throw std::logic_error("fuckup unexpected after case_end");
			}
		}
	}

	throw std::logic_error("CaseBlock reached the end");
}

void IfBlock::parseTokens(Tokens& t)
{
	while(t.cur < t.tok.size())
	{
		auto& tok = t.tok[t.cur++];
		if( not isAfterEndIf)
		{
			if(tok.type == Token::Type::kw &&
			   tok.kw() == grammar::kw::THEN)
			{
				statement.emplace_back(new Statement([](Tokens& t){
					auto& tok = t.tok[t.cur];
					LogDBG("check end if else {} {}", tok.s, (int)tok.kw());
					return tok.type == Token::Type::kw &&
							(tok.kw() == grammar::kw::ELSE || tok.kw() == grammar::kw::END_IF || tok.kw() == grammar::kw::ELSIF);
				}));
				statement.back()->parseTokens(t);

				if(t.tok[t.cur].kw() == grammar::kw::ELSIF)
				{
					++t.cur;
					cond.emplace_back(std::vector<Token>());
					continue;
				}

				if(t.tok[t.cur].kw() == grammar::kw::ELSE)
				{
					++t.cur;
					elsestatement.reset(new Statement([](Tokens& t){
						auto& tok = t.tok[t.cur];
						LogDBG("check end else {} {}", tok.s, (int)tok.kw());
						return tok.type == Token::Type::kw && tok.kw() == grammar::kw::END_IF;
					}));
					elsestatement->parseTokens(t);
				}
				++t.cur;
				isAfterEndIf = true;
			}
			else
			{
				cond.back().emplace_back(tok);
			}
		}
		else
		{
			if(tok.type == Token::Type::oper &&
			   tok.s == ";")
			{
				return;
			}
			else if(!checkNonCode(tok))
			{
				LogERR("fuckup end of IfBlock at {} {}", tok.s, tok.srcline);
				throw std::logic_error("fuckup unexpected after if_end");
			}
		}
	}

	throw std::logic_error("IfBlock reached the end");
}

int main(int argc, char **argv) {
	auto logger = spdlog::stderr_logger_st("ST2CPP");
	logger->set_level(spdlog::level::debug);
	logr = logger;
	logger->set_pattern("[%L] %v");

	auto doit = [](std::istream& srcin)
	{
		Parser p(srcin);

		try{
			auto tokens = parse(p);
			LogDBG("tokens nr:{}", tokens.size());
			for( auto& t : tokens)
			{
				t.logDump();
			}

			Unit u;
			Tokens t({0,std::move(tokens)});
			u.parseTokens(t);
			auto cpp = u.gencpp();
			for(auto& a : cpp)
				std::cout << a;
		}
		catch(std::invalid_argument e){
			LogERR("parse fuckup {} at {}: {}", e.what(), p.getfLineNr(), p.getfLine());
			return 1;
		}
		catch(std::logic_error e){
			LogERR("gen fuckup {}", e.what());
			return 1;
		}

		return 0;
	};

	if( argc > 1)
	{
		int i = 0;
		while( ++i < argc ){
			std::ifstream srcfile(argv[i]);
			auto rv = doit(srcfile);
			if(rv)
				return rv;
		};
	}
	else
	{
		return doit(std::cin);
	}

	return 0;
}
